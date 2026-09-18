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
#include <X11/Xlib.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>

#define SHM_NAME "/x11ipc_shm_all"
#define EVENT_RING_CAP 256

enum {
    OP_NOP = 0,
    OP_XOpenDisplay = 1,
    OP_XCloseDisplay,
    OP_XCreateSimpleWindow,
    OP_XMapWindow,
    OP_XDestroyWindow,
    OP_XFlush
};

typedef struct {
    uint32_t op;
    pid_t pid;
    uint32_t seq;
    uint64_t client_handle;
    union {
        struct { char name[240]; } open_display;
        struct { uint64_t handle; } close_display;
        struct { int x,y,w,h,border; unsigned long border_px, bg; } create_window;
        struct { uint64_t window_handle; } simple_window;
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
typedef int (*orig_XMapWindow_t)(Display *, Window);
typedef int (*orig_XDestroyWindow_t)(Display *, Window);
typedef int (*orig_XFlush_t)(Display *);
typedef int (*orig_XNextEvent_t)(Display *, XEvent *);
typedef int (*orig_XPending_t)(Display *);

static orig_XOpenDisplay_t orig_XOpenDisplay_real(void) { return (orig_XOpenDisplay_t)get_sym("XOpenDisplay"); }
static orig_XCloseDisplay_t orig_XCloseDisplay_real(void) { return (orig_XCloseDisplay_t)get_sym("XCloseDisplay"); }
static orig_XCreateSimpleWindow_t orig_XCreateSimpleWindow_real(void) { return (orig_XCreateSimpleWindow_t)get_sym("XCreateSimpleWindow"); }
static orig_XMapWindow_t orig_XMapWindow_real(void) { return (orig_XMapWindow_t)get_sym("XMapWindow"); }
static orig_XDestroyWindow_t orig_XDestroyWindow_real(void) { return (orig_XDestroyWindow_t)get_sym("XDestroyWindow"); }
static orig_XFlush_t orig_XFlush_real(void) { return (orig_XFlush_t)get_sym("XFlush"); }
static orig_XNextEvent_t orig_XNextEvent_real(void) { return (orig_XNextEvent_t)get_sym("XNextEvent"); }
static orig_XPending_t orig_XPending_real(void) { return (orig_XPending_t)get_sym("XPending"); }

/* helper: is IPC ready? */
static int ipc_ready(void) { return (g != NULL); }

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
    orig_XOpenDisplay_t real = orig_XOpenDisplay_real();
    if (!real) return NULL;
    Display *local = real(display_name);
    if (!local) return NULL;
    if (!ipc_ready()) return local;

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

/* XCloseDisplay: close local and request server close mapped display */
int XCloseDisplay(Display *display) {
    orig_XCloseDisplay_t real = orig_XCloseDisplay_real();
    int r = 0;
    if (real) r = real(display);
    if (!ipc_ready()) return r;

    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XCloseDisplay;
    req.client_handle = (uint64_t)(uintptr_t)display;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        fprintf(stderr, "[ipc-client] XCloseDisplay: server closed mapped display\n");
    } else {
        fprintf(stderr, "[ipc-client] XCloseDisplay: server close failed\n");
    }
    return r;
}

/* XCreateSimpleWindow: forward to server and return server Window value */
Window XCreateSimpleWindow(Display *display, Window root, int x, int y, unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border, unsigned long bg) {
    if (!ipc_ready()) {
        orig_XCreateSimpleWindow_t real = orig_XCreateSimpleWindow_real();
        return real ? real(display, root, x, y, width, height, border_width, border, bg) : (Window)0;
    }
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XCreateSimpleWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.create_window.x = x; req.p.create_window.y = y;
    req.p.create_window.w = width; req.p.create_window.h = height;
    req.p.create_window.border = border_width;
    req.p.create_window.border_px = border;
    req.p.create_window.bg = bg;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        return (Window)resp.ret_handle;
    }
    orig_XCreateSimpleWindow_t real = orig_XCreateSimpleWindow_real();
    return real ? real(display, root, x, y, width, height, border_width, border, bg) : (Window)0;
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

/* XPending: check local event ring count */
int XPending(Display *display) {
    if (!g) {
        orig_XPending_t real = orig_XPending_real();
        return real ? real(display) : 0;
    }
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return 0;
    uint32_t h = g->evt_ring.head, t = g->evt_ring.tail, c = g->evt_ring.cap;
    int cnt = (t + c - h) % c;
    pthread_mutex_unlock(&g->evt_mtx);
    return cnt;
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

/* debug constructor */
__attribute__((constructor))
static void client_debug_ctor(void) {
    fprintf(stderr, "[ipc-client] debug ctor pid=%d\n", getpid());
}

