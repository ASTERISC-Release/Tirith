/*
 client_single.c
 LD_PRELOAD client that forwards all X calls to the server. Implements:
  - On XOpenDisplay: opens a local (hidden) Display* and requests the server to open its own display mapped to the local Display* pointer.
  - On XCloseDisplay: closes the local Display* and notifies server to close its Display*.
  - All other wrapped calls (create/map/destroy/flush) are sent to server for execution and server result is returned to the client app.
  - XNextEvent/XPending: client consumes events from event ring pushed by server; before returning event to the app we set event.display to the client's local Display* so macros keep working.
 Build:
   gcc -g -O0 -Wall -Wextra -fPIC -shared -o libx11ipc.so client_single.c -ldl -lrt -pthread
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <X11/Xlib.h>

#define RPC_SHM_NAME "/x11ipc_rpc_shm"
#define REQ_SEM_NAME  "/x11ipc_req"
#define RESP_SEM_NAME "/x11ipc_resp"

#define EVENT_SHM_NAME "/x11ipc_events_shm"
#define EVENT_SEM_NAME "/x11ipc_events_sem"
#define EVENT_MTX_NAME "/x11ipc_events_mtx"

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
    uint64_t client_handle; /* local Display* repacked */
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

/* runtime objects */
static ipc_slot_t *rpc_slot = NULL;
static sem_t *req_sem = NULL;
static sem_t *resp_sem = NULL;

static event_ring_t *evt_ring = NULL;
static sem_t *evt_sem = NULL;
static sem_t *evt_mtx = NULL;

static uint32_t seq_counter = 1;
static pid_t mypid = 0;

/* dynamic lookup of real libX11 functions */
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

/* helpers */
static int ipc_ready(void) {
    return (rpc_slot != NULL && req_sem != NULL && resp_sem != NULL);
}

/* client constructor: map rpc slot & semaphores & event ring */
static void __attribute__((constructor)) client_init(void) {
    mypid = getpid();
    int fd = shm_open(RPC_SHM_NAME, O_RDWR, 0);
    if (fd < 0) { fprintf(stderr, "[ipc-client] cannot open RPC shm: %s\n", strerror(errno)); return; }
    rpc_slot = mmap(NULL, sizeof(ipc_slot_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (rpc_slot == MAP_FAILED) { fprintf(stderr, "[ipc-client] rpc mmap failed: %s\n", strerror(errno)); rpc_slot = NULL; close(fd); return; }

    req_sem = sem_open(REQ_SEM_NAME, 0);
    if (req_sem == SEM_FAILED) { fprintf(stderr, "[ipc-client] sem_open req failed: %s\n", strerror(errno)); req_sem = NULL; }
    resp_sem = sem_open(RESP_SEM_NAME, 0);
    if (resp_sem == SEM_FAILED) { fprintf(stderr, "[ipc-client] sem_open resp failed: %s\n", strerror(errno)); resp_sem = NULL; }

    int efd = shm_open(EVENT_SHM_NAME, O_RDWR, 0);
    if (efd >= 0) {
        evt_ring = mmap(NULL, sizeof(event_ring_t), PROT_READ|PROT_WRITE, MAP_SHARED, efd, 0);
        if (evt_ring == MAP_FAILED) { fprintf(stderr,"[ipc-client] evt mmap failed: %s\n", strerror(errno)); evt_ring = NULL; }
    } else evt_ring = NULL;

    evt_sem = sem_open(EVENT_SEM_NAME, 0); if (evt_sem == SEM_FAILED) evt_sem = NULL;
    evt_mtx = sem_open(EVENT_MTX_NAME, 0); if (evt_mtx == SEM_FAILED) evt_mtx = NULL;

    fprintf(stderr, "[ipc-client] init pid=%d rpc_slot=%p req_sem=%p resp_sem=%p evt_ring=%p\n",
            mypid, (void*)rpc_slot, (void*)req_sem, (void*)resp_sem, (void*)evt_ring);
}

/* destructor: cleanup handles */
static void __attribute__((destructor)) client_fini(void) {
    if (rpc_slot && rpc_slot != MAP_FAILED) munmap((void*)rpc_slot, sizeof(ipc_slot_t));
    if (evt_ring && evt_ring != MAP_FAILED) munmap((void*)evt_ring, sizeof(event_ring_t));
    if (req_sem && req_sem != SEM_FAILED) sem_close(req_sem);
    if (resp_sem && resp_sem != SEM_FAILED) sem_close(resp_sem);
    if (evt_sem && evt_sem != SEM_FAILED) sem_close(evt_sem);
    if (evt_mtx && evt_mtx != SEM_FAILED) sem_close(evt_mtx);
}

/* synchronous RPC: write request (including client_handle) and wait for response */
static int send_request_and_wait(ipc_slot_t *req, ipc_slot_t *resp_out) {
    if (!ipc_ready()) return -1;
    req->pid = mypid;
    req->seq = __sync_fetch_and_add(&seq_counter, 1);
    memcpy(rpc_slot, req, sizeof(*req));
    if (sem_post(req_sem) != 0) return -1;
    if (sem_wait(resp_sem) != 0) return -1;
    memcpy(resp_out, rpc_slot, sizeof(*resp_out));
    return 0;
}

/* ---- wrappers ---- */

/* XOpenDisplay: open a local (hidden) Display* for the client, then request the server open its own display and map it */
Display *XOpenDisplay(const char *display_name) {
    orig_XOpenDisplay_t real = orig_XOpenDisplay_real();
    if (!real) return NULL;

    /* open local display so macros like DefaultScreen(dpy) work in the client process */
    Display *local_dpy = real(display_name);
    if (!local_dpy) {
        fprintf(stderr, "[ipc-client] local XOpenDisplay failed; falling back\n");
        return NULL;
    }

    /* if server is available, request server to open a display and map client_handle->server Display* */
    if (ipc_ready()) {
        ipc_slot_t req; memset(&req, 0, sizeof(req));
        req.op = OP_XOpenDisplay;
        req.client_handle = (uint64_t)(uintptr_t)local_dpy;
        if (display_name) strncpy(req.p.open_display.name, display_name, sizeof(req.p.open_display.name)-1);
        ipc_slot_t resp;
        if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
            fprintf(stderr, "[ipc-client] XOpenDisplay: server opened display (handle=0x%016" PRIx64 ")\n", resp.ret_handle);
        } else {
            fprintf(stderr, "[ipc-client] XOpenDisplay: server open failed or not available\n");
        }
    }
    return local_dpy;
}

/* XCloseDisplay: close local display and request server to close its mapped display */
int XCloseDisplay(Display *display) {
    orig_XCloseDisplay_t real = orig_XCloseDisplay_real();
    int r = 0;
    if (real) r = real(display);

    if (ipc_ready()) {
        ipc_slot_t req; memset(&req,0,sizeof(req));
        req.op = OP_XCloseDisplay;
        req.client_handle = (uint64_t)(uintptr_t)display;
        ipc_slot_t resp;
        if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
            fprintf(stderr, "[ipc-client] XCloseDisplay: server closed mapped display\n");
        } else {
            fprintf(stderr, "[ipc-client] XCloseDisplay: server close failed or not available\n");
        }
    }
    return r;
}

/* XCreateSimpleWindow: forward to server and return server Window value to client app */
Window XCreateSimpleWindow(Display *display, Window root, int x, int y, unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border, unsigned long bg) {
    /* do not create a local window; forward request to server */
    if (!ipc_ready()) {
        /* fallback: call real function locally so program still runs */
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
    /* fallback */
    orig_XCreateSimpleWindow_t real = orig_XCreateSimpleWindow_real();
    return real ? real(display, root, x, y, width, height, border_width, border, bg) : (Window)0;
}

/* XMapWindow: forward to server */
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
    if (send_request_and_wait(&req, &resp) == 0) return resp.status == 0 ? 0 : 1;
    orig_XMapWindow_t real = orig_XMapWindow_real();
    return real ? real(display, w) : 0;
}

/* XDestroyWindow: forward to server */
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
    if (send_request_and_wait(&req, &resp) == 0) return resp.status == 0 ? 0 : 1;
    orig_XDestroyWindow_t real = orig_XDestroyWindow_real();
    return real ? real(display, w) : 0;
}

/* XFlush: forward to server */
int XFlush(Display *display) {
    if (!ipc_ready()) {
        orig_XFlush_t real = orig_XFlush_real();
        return real ? real(display) : 0;
    }
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XFlush;
    req.client_handle = (uint64_t)(uintptr_t)display;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status == 0 ? 0 : 1;
    orig_XFlush_t real = orig_XFlush_real();
    return real ? real(display) : 0;
}

/* XPending: check local event ring */
int XPending(Display *display) {
    if (!evt_ring || !evt_mtx) {
        orig_XPending_t real = orig_XPending_real();
        return real ? real(display) : 0;
    }
    if (sem_wait(evt_mtx) != 0) return 0;
    uint32_t h = evt_ring->head, t = evt_ring->tail, c = evt_ring->cap;
    int cnt = (t + c - h) % c;
    sem_post(evt_mtx);
    return cnt;
}

/* XNextEvent: read next event from shared ring, set event.display to the client's local Display* before returning */
int XNextEvent(Display *display, XEvent *event_return) {
    if (!evt_ring || !evt_sem || !evt_mtx) {
        orig_XNextEvent_t real = orig_XNextEvent_real();
        return real ? real(display, event_return) : 0;
    }
    /* block until server posts an event */
    if (sem_wait(evt_sem) != 0) return 0;
    if (sem_wait(evt_mtx) != 0) return 0;
    if (evt_ring->head == evt_ring->tail) { sem_post(evt_mtx); return 0; }
    XEvent ev = evt_ring->events[evt_ring->head];
    evt_ring->head = (evt_ring->head + 1) % evt_ring->cap;
    sem_post(evt_mtx);

    /* set event.display to the client's local Display* so macros work */
    ev.xany.display = display;
    *event_return = ev;
    return 0;
}

/* debug ctor */
__attribute__((constructor))
static void client_debug_ctor(void) {
    fprintf(stderr, "[ipc-client] debug ctor pid=%d\n", getpid());
}

