/* client_shm.c
   LD_PRELOAD client that forwards X calls to the server via single shared memory region
   using pthread mutex+cond for synchronization (no sem_* syscalls).

   Build:
     gcc -g -O0 -Wall -Wextra -fPIC -shared -o libx11ipc.so client_shm.c -ldl -lrt -pthread
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define SHM_NAME "/x11ipc_shm_all"
#define EVENT_RING_CAP 256

enum {
    OP_NOP = 0,
    OP_XOpenDisplay = 1,
    OP_XCloseDisplay,
    OP_XCreateSimpleWindow,
    OP_XCreateWindow,
    OP_XMapWindow,
    OP_XUnmapWindow,
    OP_XConfigureWindow,
    OP_XDestroyWindow,
    OP_XFlush,
    OP_XSelectInput
};

typedef struct {
    uint32_t op;
    pid_t pid;
    uint32_t seq;
    uint64_t client_handle;
    union {
        struct { char name[240]; } open_display;
        struct { uint64_t handle; } close_display;
        /* create_simple kept for backward compatibility */
        struct { int x,y,w,h,border; unsigned long border_px, bg; } create_window_simple;

        /* full create window */
        struct {
            uint64_t parent;         /* Window parent (opaque numeric handle) */
            int x, y;
            unsigned int width, height;
            unsigned int border_width;
            int depth;
            unsigned int class;      /* InputOutput, InputOnly, etc. */
            uint64_t visual;         /* Visual* as opaque uint64 (may be NULL/0) */
            unsigned long valuemask; /* valuemask bits (CWBackPixel, CWBorderPixel, CWEventMask, ...) */
            unsigned long background_pixel;
            unsigned long border_pixel;
            unsigned long event_mask;
        } create_window;

        struct { uint64_t window_handle; unsigned long event_mask; } select_input;
        struct { uint64_t window_handle; } simple_window;

        /* unmap */
        struct { uint64_t window_handle; } unmap_window;

        /* configure */
        struct {
            uint64_t window_handle;
            unsigned long value_mask;
            int x, y;
            unsigned int width, height;
            unsigned int border_width;
            uint64_t sibling;
            int stack_mode;
        } configure_window;
    } p;
    int32_t ret_int;
    uint64_t ret_handle;
    int8_t status;
} ipc_slot_t;

typedef struct {
    uint32_t head;
    uint32_t tail;
    uint32_t cap;
    XEvent events[EVENT_RING_CAP];
} event_ring_t;

typedef struct {
    pthread_mutex_t req_mtx;
    pthread_cond_t  req_cond;
    int req_ready;
    int resp_ready;
    ipc_slot_t rpc_slot;
    pthread_mutex_t evt_mtx;
    pthread_cond_t  evt_cond;
    event_ring_t evt_ring;
} shared_region_t;

/* runtime pointers */
static shared_region_t *g = NULL;
static pid_t mypid = 0;
static uint32_t seq_counter = 1;

/* lookup real libX11 symbols */
static void *get_sym(const char *name) {
    void *s = dlsym(RTLD_NEXT, name);
    if (s) return s;
    void *h = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    if (!h) h = dlopen("libX11.so", RTLD_LAZY | RTLD_LOCAL);
    if (h) return dlsym(h, name);
    return NULL;
}
typedef Display *(*orig_XOpenDisplay_t)(const char *);
typedef int (*orig_XCloseDisplay_t)(Display *);
typedef Window (*orig_XCreateSimpleWindow_t)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long);
typedef Window (*orig_XCreateWindow_t)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *);
typedef int (*orig_XMapWindow_t)(Display *, Window);
typedef int (*orig_XUnmapWindow_t)(Display *, Window);
typedef int (*orig_XConfigureWindow_t)(Display *, Window, unsigned int, XWindowChanges *);
typedef int (*orig_XDestroyWindow_t)(Display *, Window);
typedef int (*orig_XFlush_t)(Display *);
typedef int (*orig_XNextEvent_t)(Display *, XEvent *);
typedef int (*orig_XPending_t)(Display *);
typedef int (*orig_XSelectInput_t)(Display *, Window, long);

static orig_XOpenDisplay_t orig_XOpenDisplay_real(void) { return (orig_XOpenDisplay_t)get_sym("XOpenDisplay"); }
static orig_XCloseDisplay_t orig_XCloseDisplay_real(void) { return (orig_XCloseDisplay_t)get_sym("XCloseDisplay"); }
static orig_XCreateSimpleWindow_t orig_XCreateSimpleWindow_real(void) { return (orig_XCreateSimpleWindow_t)get_sym("XCreateSimpleWindow"); }
static orig_XCreateWindow_t orig_XCreateWindow_real(void) { return (orig_XCreateWindow_t)get_sym("XCreateWindow"); }
static orig_XMapWindow_t orig_XMapWindow_real(void) { return (orig_XMapWindow_t)get_sym("XMapWindow"); }
static orig_XUnmapWindow_t orig_XUnmapWindow_real(void) { return (orig_XUnmapWindow_t)get_sym("XUnmapWindow"); }
static orig_XConfigureWindow_t orig_XConfigureWindow_real(void) { return (orig_XConfigureWindow_t)get_sym("XConfigureWindow"); }
static orig_XDestroyWindow_t orig_XDestroyWindow_real(void) { return (orig_XDestroyWindow_t)get_sym("XDestroyWindow"); }
static orig_XFlush_t orig_XFlush_real(void) { return (orig_XFlush_t)get_sym("XFlush"); }
static orig_XNextEvent_t orig_XNextEvent_real(void) { return (orig_XNextEvent_t)get_sym("XNextEvent"); }
static orig_XPending_t orig_XPending_real(void) { return (orig_XPending_t)get_sym("XPending"); }
static orig_XSelectInput_t orig_XSelectInput_real(void) { return (orig_XSelectInput_t)get_sym("XSelectInput"); }

/* Add at the top with other globals */
static int unsupported_call_made = 0;
static pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Add a function to handle unsupported calls */
static void handle_unsupported_call(const char *function_name) {
    pthread_mutex_lock(&exit_mutex);
    if (!unsupported_call_made) {
        unsupported_call_made = 1;
        fprintf(stderr, "[ipc-client] ERROR: Unsupported X11 call '%s' - exiting client\n", function_name);
        pthread_mutex_unlock(&exit_mutex);
        
        /* Try to cleanup gracefully */
        if (g) {
            munmap((void*)g, sizeof(*g));
            g = NULL;
        }
        
        /* Exit the process */
        _exit(127); /* Use _exit to avoid atexit handlers that might call X11 functions */
    }
    pthread_mutex_unlock(&exit_mutex);
}

/* Add a macro for unsupported functions */
#define UNSUPPORTED_CALL() handle_unsupported_call(__func__)

/* helper: is IPC ready? */
static int ipc_ready(void) { return (g != NULL); }

/* manage local Display* pointers */
#define NUM_DISPLAYS 16
Display* localDisplayPointers[NUM_DISPLAYS];
int numLocalDisplays = 0;
Display* get_local_display_pointer(void) {
    for (int i = 0; i < NUM_DISPLAYS; i++) {
        if (localDisplayPointers[i] == NULL) {
            localDisplayPointers[i] = malloc(4096); // Allocate memory for Display
            return localDisplayPointers[i];
        }
    }
    fprintf(stderr, "[ipc-client] ERROR: Exceeded maximum number of local Display pointers (%d)\n", NUM_DISPLAYS);
    return NULL; // No more available slots
}
void put_local_display_pointer(Display* dpy) {
    for (int i = 0; i < NUM_DISPLAYS; i++) {
        if (localDisplayPointers[i] == dpy) {
            localDisplayPointers[i] = NULL;
            numLocalDisplays--;
            return;
        }
    }
    fprintf(stderr, "[ipc-client] WARNING: Attempted to remove unknown Display pointer %p\n", (void*)dpy);
}


/* map shared memory in constructor */
static void __attribute__((constructor)) client_init(void) {
    mypid = getpid();

    /* open with read-write so we can write req flags & rpc_slot */
    int fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (fd < 0) {
        /* server may not be running; graceful fallback */
        fprintf(stderr, "[ipc-client] cannot open shared region %s: %s (falling back to local Xlib)\n",
                SHM_NAME, strerror(errno));
        return;
    }

    void *p = mmap(NULL, sizeof(*g), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        /* if mmap fails, show clear diagnostics and fallback */
        fprintf(stderr, "[ipc-client] mmap failed for %s: %s (falling back)\n", SHM_NAME, strerror(errno));
        close(fd);
        return;
    }

    for (int i = 0; i < NUM_DISPLAYS; i++) {
        localDisplayPointers[i] = NULL;
    }

    /* success */
    g = (shared_region_t*)p;
    fprintf(stderr, "[ipc-client] init pid=%d shared=%s ptr=%p\n", mypid, SHM_NAME, (void*)g);
}

/* destructor: unmap */
static void __attribute__((destructor)) client_fini(void) {
    if (g) munmap((void*)g, sizeof(*g));
}

/* synchronous RPC: write request and wait for response using req_mtx/req_cond */
static int send_request_and_wait(ipc_slot_t *req, ipc_slot_t *resp_out) {
    if (!ipc_ready()) return -1;
    req->pid = mypid;
    req->seq = __sync_fetch_and_add(&seq_counter, 1);

    /* write request under req_mtx */
    if (pthread_mutex_lock(&g->req_mtx) != 0) return -1;
    memcpy(&g->rpc_slot, req, sizeof(*req));
    g->req_ready = 1;
    g->resp_ready = 0;
    pthread_cond_signal(&g->req_cond);
    /* wait for server to set resp_ready */
    while (!g->resp_ready) {
        pthread_cond_wait(&g->req_cond, &g->req_mtx);
    }
    memcpy(resp_out, &g->rpc_slot, sizeof(*resp_out));
    /* reset flags */
    g->resp_ready = 0;
    g->req_ready = 0;
    pthread_mutex_unlock(&g->req_mtx);
    return 0;
}

/* event helpers */
static int event_count(void) {
    if (!g) return 0;
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return 0;
    uint32_t h = g->evt_ring.head, t = g->evt_ring.tail, c = g->evt_ring.cap;
    int cnt = (t + c - h) % c;
    pthread_mutex_unlock(&g->evt_mtx);
    return cnt;
}

/* pop next event (blocks until available) */
static int pop_event(XEvent *out) {
    if (!g) return -1;
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return -1;
    while (g->evt_ring.head == g->evt_ring.tail) {
        pthread_cond_wait(&g->evt_cond, &g->evt_mtx);
    }
    *out = g->evt_ring.events[g->evt_ring.head];
    g->evt_ring.head = (g->evt_ring.head + 1) % g->evt_ring.cap;
    pthread_mutex_unlock(&g->evt_mtx);
    return 0;
}

/* ---- wrappers ---- */

/* XOpenDisplay: open local Display* and request server to open server-side display */
Display *XOpenDisplay(const char *display_name) {
    // orig_XOpenDisplay_t real = orig_XOpenDisplay_real();
    // if (!real) return NULL;
    // Display *local = real(display_name);
    Display *local = get_local_display_pointer();
    if (!local) return NULL;
    // if (!ipc_ready()) return local;

    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XOpenDisplay;
    req.client_handle = (uint64_t)(uintptr_t)local;
    if (display_name) strncpy(req.p.open_display.name, display_name, sizeof(req.p.open_display.name)-1);
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        fprintf(stderr, "[ipc-client] XOpenDisplay: server opened (ret=%" PRIu64 ")\n", resp.ret_handle);
    } else {
        fprintf(stderr, "[ipc-client] XOpenDisplay: server open failed\n");
    }
    return local;
}

int XDefaultScreen(Display *display) {
    /* Always return 0 as requested */
    fprintf(stderr, "[ipc-client] XDefaultScreen: returning 0\n");
    return 0;
}

/* XCloseDisplay: close local and request server close mapped display */
int XCloseDisplay(Display *display) {
    // orig_XCloseDisplay_t real = orig_XCloseDisplay_real();
    // int r = 0;
    // if (real) r = real(display);
    // if (!ipc_ready()) return r;
    put_local_display_pointer(display);

    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XCloseDisplay;
    req.client_handle = (uint64_t)(uintptr_t)display;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        fprintf(stderr, "[ipc-client] XCloseDisplay: server closed mapped display\n");
    } else {
        fprintf(stderr, "[ipc-client] XCloseDisplay: server close failed\n");
    }
    return 0;
}

/* XCreateSimpleWindow: forward to server and return server Window value */
Window XCreateSimpleWindow(Display *display, Window root, int x, int y, unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border, unsigned long bg) {
    // if (!ipc_ready()) {
    //     orig_XCreateSimpleWindow_t real = orig_XCreateSimpleWindow_real();
    //     return real ? real(display, root, x, y, width, height, border_width, border, bg) : (Window)0;
    // }
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XCreateSimpleWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.create_window_simple.x = x; req.p.create_window_simple.y = y;
    req.p.create_window_simple.w = width; req.p.create_window_simple.h = height;
    req.p.create_window_simple.border = border_width;
    req.p.create_window_simple.border_px = border;
    req.p.create_window_simple.bg = bg;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        return (Window)resp.ret_handle;
    }
    // orig_XCreateSimpleWindow_t real = orig_XCreateSimpleWindow_real();
    // return real ? real(display, root, x, y, width, height, border_width, border, bg) : (Window)0;
    return (Window)0;
}

/* New: XCreateWindow (full) */
Window XCreateWindow(Display *display, Window parent, int x, int y,
                     unsigned int width, unsigned int height, unsigned int border_width,
                     int depth, unsigned int class, Visual *visual,
                     unsigned long valuemask, XSetWindowAttributes *attributes) {

    orig_XCreateWindow_t real = orig_XCreateWindow_real();

    if (!ipc_ready()) {
        return real ? real(display, parent, x, y, width, height, border_width, depth, class, visual, valuemask, attributes) : (Window)0;
    }

    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XCreateWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.create_window.parent = (uint64_t)(uintptr_t)parent;
    req.p.create_window.x = x;
    req.p.create_window.y = y;
    req.p.create_window.width = width;
    req.p.create_window.height = height;
    req.p.create_window.border_width = border_width;
    req.p.create_window.depth = depth;
    req.p.create_window.class = class;
    req.p.create_window.visual = (uint64_t)(uintptr_t)visual;
    req.p.create_window.valuemask = valuemask;

    if (attributes) {
        if (valuemask & CWBackPixel) req.p.create_window.background_pixel = attributes->background_pixel;
        if (valuemask & CWBorderPixel) req.p.create_window.border_pixel = attributes->border_pixel;
        if (valuemask & CWEventMask) req.p.create_window.event_mask = attributes->event_mask;
    } else {
        req.p.create_window.background_pixel = 0;
        req.p.create_window.border_pixel = 0;
        req.p.create_window.event_mask = 0;
    }

    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        return (Window)resp.ret_handle;
    }

    /* fallback to local XCreateWindow if remote failed */
    return real ? real(display, parent, x, y, width, height, border_width, depth, class, visual, valuemask, attributes) : (Window)0;
}

/* XMapWindow: forward */
int XMapWindow(Display *display, Window w) {
    if (!ipc_ready()) {
        orig_XMapWindow_t real = orig_XMapWindow_real();
        return real ? real(display, w) : 0;
    }
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XMapWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.simple_window.window_handle = (uint64_t)w;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    orig_XMapWindow_t real = orig_XMapWindow_real();
    return real ? real(display, w) : 0;
}

/* New: XUnmapWindow */
int XUnmapWindow(Display *display, Window w) {
    orig_XUnmapWindow_t real = orig_XUnmapWindow_real();
    if (!ipc_ready()) {
        return real ? real(display, w) : 0;
    }
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XUnmapWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.unmap_window.window_handle = (uint64_t)w;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return real ? real(display, w) : 0;
}

/* New: XConfigureWindow */
int XConfigureWindow(Display *display, Window w, unsigned int value_mask, XWindowChanges *changes) {
    orig_XConfigureWindow_t real = orig_XConfigureWindow_real();
    if (!ipc_ready()) {
        return real ? real(display, w, value_mask, changes) : 0;
    }
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XConfigureWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.configure_window.window_handle = (uint64_t)w;
    req.p.configure_window.value_mask = value_mask;
    if (changes) {
        /* copy fields: only common ones are stored */
        req.p.configure_window.x = changes->x;
        req.p.configure_window.y = changes->y;
        req.p.configure_window.width = changes->width;
        req.p.configure_window.height = changes->height;
        req.p.configure_window.border_width = changes->border_width;
        req.p.configure_window.sibling = (uint64_t)(uintptr_t)changes->sibling;
        req.p.configure_window.stack_mode = changes->stack_mode;
    } else {
        req.p.configure_window.x = req.p.configure_window.y = 0;
        req.p.configure_window.width = req.p.configure_window.height = 0;
        req.p.configure_window.border_width = 0;
        req.p.configure_window.sibling = 0;
        req.p.configure_window.stack_mode = 0;
    }
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return real ? real(display, w, value_mask, changes) : 0;
}

/* XDestroyWindow: forward */
int XDestroyWindow(Display *display, Window w) {
    if (!ipc_ready()) {
        orig_XDestroyWindow_t real = orig_XDestroyWindow_real();
        return real ? real(display, w) : 0;
    }
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XDestroyWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.simple_window.window_handle = (uint64_t)w;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    orig_XDestroyWindow_t real = orig_XDestroyWindow_real();
    return real ? real(display, w) : 0;
}

/* XFlush: forward */
int XFlush(Display *display) {
    if (!ipc_ready()) {
        orig_XFlush_t real = orig_XFlush_real();
        return real ? real(display) : 0;
    }
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XFlush;
    req.client_handle = (uint64_t)(uintptr_t)display;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    orig_XFlush_t real = orig_XFlush_real();
    return real ? real(display) : 0;
}

/* New: XSelectInput */
int XSelectInput(Display *display, Window w, long event_mask) {
    orig_XSelectInput_t real = orig_XSelectInput_real();
    if (!ipc_ready()) {
        return real ? real(display, w, event_mask) : 0;
    }
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XSelectInput;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.select_input.window_handle = (uint64_t)w;
    req.p.select_input.event_mask = (unsigned long)event_mask;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return real ? real(display, w, event_mask) : 0;
}

/* XPending: check local event ring count */
int XPending(Display *display) {
    if (!g) {
        orig_XPending_t real = orig_XPending_real();
        return real ? real(display) : 0;
    }
    return event_count(); // Use your helper function
}

/* XNextEvent: pop from shared event ring, rewrite display pointer */
int XNextEvent(Display *display, XEvent *event_return) {
    if (!g) {
        orig_XNextEvent_t real = orig_XNextEvent_real();
        return real ? real(display, event_return) : 0;
    }
    XEvent ev;
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return 0;
    while (g->evt_ring.head == g->evt_ring.tail) {
        pthread_cond_wait(&g->evt_cond, &g->evt_mtx);
    }
    ev = g->evt_ring.events[g->evt_ring.head];
    g->evt_ring.head = (g->evt_ring.head + 1) % g->evt_ring.cap;
    pthread_mutex_unlock(&g->evt_mtx);

    /* rewrite */
    ev.xany.display = display;
    *event_return = ev;
    return 0;
}

/* Unsupported X11 calls: log and exit */
Window XDefaultRootWindow(Display *display) {
    UNSUPPORTED_CALL();
    return (Window)0; /* Never reached */
}

Window XRootWindow(Display *display, int screen_number) {
    UNSUPPORTED_CALL();
    return (Window)0; /* Never reached */
}

Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return None;
}

int XChangeProperty(Display *display, Window w, Atom property, Atom type,
                    int format, int mode, const unsigned char *data, int nelements) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

Status XSetWMProtocols(Display *display, Window w, Atom *protocols, int count) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XStoreName(Display *display, Window w, const char *window_name) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XSetWMHints(Display *display, Window w, XWMHints *wmhints) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

XWMHints *XAllocWMHints(void) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XFree(void *data) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XSetStandardProperties(Display *display, Window w, const char *window_name,
                          const char *icon_name, Pixmap icon_pixmap,
                          char **argv, int argc, XSizeHints *hints) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer,
                  KeySym *keysym_return, XComposeStatus *status_in_out) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XEventsQueued(Display *display, int mode) {
    /* Same as XPending for our purposes */
    return event_count();
}

int XConnectionNumber(Display *display) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return -1;
}

Status XGetGeometry(Display *display, Drawable d, Window *root_return,
                   int *x_return, int *y_return, unsigned int *width_return,
                   unsigned int *height_return, unsigned int *border_width_return,
                   unsigned int *depth_return) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XMoveWindow(Display *display, Window w, int x, int y) {
    /* Use XConfigureWindow to move the window */
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XConfigureWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.configure_window.window_handle = (uint64_t)w;
    req.p.configure_window.value_mask = CWX | CWY;
    req.p.configure_window.x = x;
    req.p.configure_window.y = y;
    req.p.configure_window.width = 0;
    req.p.configure_window.height = 0;
    req.p.configure_window.border_width = 0;
    req.p.configure_window.sibling = 0;
    req.p.configure_window.stack_mode = 0;
    
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;

    fprintf(stderr, "[ERROR] XMoveWindow: Should not come here.\n");
    return 0;
}

int XResizeWindow(Display *display, Window w, unsigned int width, unsigned int height) {
    /* Use XConfigureWindow to resize the window */
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XConfigureWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.configure_window.window_handle = (uint64_t)w;
    req.p.configure_window.value_mask = CWWidth | CWHeight;
    req.p.configure_window.x = 0;
    req.p.configure_window.y = 0;
    req.p.configure_window.width = width;
    req.p.configure_window.height = height;
    req.p.configure_window.border_width = 0;
    req.p.configure_window.sibling = 0;
    req.p.configure_window.stack_mode = 0;
    
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
   
    fprintf(stderr, "[ERROR] XResizeWindow: Should not come here.\n");
    return 0;
}

/* debug constructor */
__attribute__((constructor))
static void client_debug_ctor(void) {
    fprintf(stderr, "[ipc-client] debug ctor pid=%d\n", getpid());
}

