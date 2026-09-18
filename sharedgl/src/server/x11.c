/* server_shm.c
   Single-client X11 IPC server using one large shared memory region with
   process-shared pthread mutexes and condition variables (no sem_* syscalls).

   Build:
     gcc -g -O0 -Wall -Wextra -o server_shm server_shm.c -lX11 -pthread -lrt
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <errno.h>
#include <inttypes.h>

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
        struct { int x,y,w,h,border; unsigned long border_px, bg; } create_window_simple;

        struct {
            uint64_t parent;
            int x,y;
            unsigned int width, height;
            unsigned int border_width;
            int depth;
            unsigned int class;
            uint64_t visual;
            unsigned long valuemask;
            unsigned long background_pixel;
            unsigned long border_pixel;
            unsigned long event_mask;
        } create_window;

        struct { uint64_t window_handle; unsigned long event_mask; } select_input;
        struct { uint64_t window_handle; } simple_window;

        struct { uint64_t window_handle; } unmap_window;

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

/* Shared memory layout */
typedef struct {
    /* request/response synchronization */
    pthread_mutex_t req_mtx;
    pthread_cond_t  req_cond;
    int req_ready;   /* set to 1 by client when a request is ready */
    int resp_ready;  /* set to 1 by server when response is ready */

    ipc_slot_t rpc_slot;

    /* event ring and synchronization */
    pthread_mutex_t evt_mtx;
    pthread_cond_t  evt_cond;
    event_ring_t evt_ring;
} shared_region_t;

/* server-side maps */
typedef struct DisplayEntry {
    uint64_t client_handle;
    Display *dpy;
    struct DisplayEntry *next;
} DisplayEntry;

static shared_region_t *g = NULL;
static DisplayEntry *display_map = NULL;
static pthread_mutex_t maps_lock = PTHREAD_MUTEX_INITIALIZER;

static Display *lookup_server_display(uint64_t client_handle) {
    Display *ret = NULL;
    pthread_mutex_lock(&maps_lock);
    for (DisplayEntry *e = display_map; e; e = e->next) {
        if (e->client_handle == client_handle) { ret = e->dpy; break; }
    }
    pthread_mutex_unlock(&maps_lock);
    return ret;
}
static void map_server_display(uint64_t client_handle, Display *dpy) {
    DisplayEntry *e = calloc(1, sizeof(*e));
    e->client_handle = client_handle; e->dpy = dpy;
    pthread_mutex_lock(&maps_lock);
    e->next = display_map; display_map = e;
    pthread_mutex_unlock(&maps_lock);
}
static void unmap_server_display(uint64_t client_handle) {
    pthread_mutex_lock(&maps_lock);
    DisplayEntry **pp = &display_map;
    while (*pp) {
        if ((*pp)->client_handle == client_handle) {
            DisplayEntry *t = *pp;
            *pp = t->next;
            XCloseDisplay(t->dpy);
            free(t);
            break;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&maps_lock);
}

/* event push */
static int push_event(const XEvent *ev) {
    if (!g) return -1;
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return -1;
    uint32_t next_tail = (g->evt_ring.tail + 1) % g->evt_ring.cap;
    if (next_tail == g->evt_ring.head) {
        /* full */
        pthread_mutex_unlock(&g->evt_mtx);
        return -1;
    }
    g->evt_ring.events[g->evt_ring.tail] = *ev;
    g->evt_ring.tail = next_tail;
    pthread_cond_signal(&g->evt_cond);
    pthread_mutex_unlock(&g->evt_mtx);
    return 0;
}

/* process a single RPC */
static void process_rpc(const ipc_slot_t *req, ipc_slot_t *resp) {
    memset(resp, 0, sizeof(*resp));
    resp->op = req->op; resp->pid = req->pid; resp->seq = req->seq; resp->status = 1;

    switch (req->op) {
    case OP_XOpenDisplay: {
        const char *name = req->p.open_display.name;
        Display *dpy = XOpenDisplay(name[0] ? name : NULL);
        if (!dpy) { resp->status = 1; }
        else {
            map_server_display(req->client_handle, dpy);
            resp->status = 0;
            resp->ret_handle = 1; /* opaque success */
        }
        break;
    }
    case OP_XCloseDisplay: {
        Display *d = lookup_server_display(req->client_handle);
        if (!d) { resp->status = 1; break; }
        unmap_server_display(req->client_handle);
        resp->status = 0;
        break;
    }
    case OP_XCreateSimpleWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window root = DefaultRootWindow(dpy);
        Window w = XCreateSimpleWindow(dpy, root,
                                       req->p.create_window_simple.x, req->p.create_window_simple.y,
                                       req->p.create_window_simple.w, req->p.create_window_simple.h,
                                       req->p.create_window_simple.border,
                                       req->p.create_window_simple.border_px,
                                       req->p.create_window_simple.bg);
        resp->status = 0;
        resp->ret_handle = (uint64_t)w;
        break;
    }
    case OP_XCreateWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window parent = (Window)req->p.create_window.parent;
        XSetWindowAttributes swa;
        memset(&swa, 0, sizeof(swa));
        unsigned long valuemask = 0;

        if (req->p.create_window.valuemask & CWBackPixel) {
            swa.background_pixel = req->p.create_window.background_pixel;
            valuemask |= CWBackPixel;
        }
        if (req->p.create_window.valuemask & CWBorderPixel) {
            swa.border_pixel = req->p.create_window.border_pixel;
            valuemask |= CWBorderPixel;
        }
        if (req->p.create_window.valuemask & CWEventMask) {
            swa.event_mask = req->p.create_window.event_mask;
            valuemask |= CWEventMask;
        }

        /* visual pointer: cast back to Visual* if non-zero. */
        Visual *visual = NULL;
        if (req->p.create_window.visual != 0) visual = (Visual *)(uintptr_t)req->p.create_window.visual;

        Window w = XCreateWindow(dpy, parent,
                                 req->p.create_window.x, req->p.create_window.y,
                                 req->p.create_window.width, req->p.create_window.height,
                                 req->p.create_window.border_width,
                                 req->p.create_window.depth,
                                 req->p.create_window.class,
                                 visual,
                                 valuemask,
                                 &swa);
        resp->status = 0;
        resp->ret_handle = (uint64_t)w;
        break;
    }
    case OP_XMapWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.simple_window.window_handle;
        XMapWindow(dpy, w);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XUnmapWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.unmap_window.window_handle;
        XUnmapWindow(dpy, w);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XConfigureWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.configure_window.window_handle;
        unsigned long mask = req->p.configure_window.value_mask;
        XWindowChanges wc;
        memset(&wc, 0, sizeof(wc));
        if (mask & CWX) wc.x = req->p.configure_window.x;
        if (mask & CWY) wc.y = req->p.configure_window.y;
        if (mask & CWWidth) wc.width = req->p.configure_window.width;
        if (mask & CWHeight) wc.height = req->p.configure_window.height;
        if (mask & CWBorderWidth) wc.border_width = req->p.configure_window.border_width;
        if (mask & CWSibling) wc.sibling = (Window)req->p.configure_window.sibling;
        if (mask & CWStackMode) wc.stack_mode = req->p.configure_window.stack_mode;

        XConfigureWindow(dpy, w, mask, &wc);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XDestroyWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.simple_window.window_handle;
        XDestroyWindow(dpy, w);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XFlush: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XSelectInput: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.select_input.window_handle;
        long mask = (long)req->p.select_input.event_mask;
        XSelectInput(dpy, w, mask);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    default:
        resp->status = 1;
        break;
    }
}

/* pull events from server displays and push into shared ring */
static void dispatch_events_all(void) {
    pthread_mutex_lock(&maps_lock);
    for (DisplayEntry *e = display_map; e; e = e->next) {
        Display *dpy = e->dpy;
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            ev.xany.display = NULL; /* client will set local display pointer */
            if (push_event(&ev) != 0) {
                fprintf(stderr, "server_shm: event ring full; dropping event\n");
            }
        }
    }
    pthread_mutex_unlock(&maps_lock);
}

void* x11_main(void* ptr) {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) { perror("shm_open"); return NULL; }
    if (ftruncate(fd, sizeof(shared_region_t)) != 0) { perror("ftruncate"); return NULL; }
    void *p = mmap(NULL, sizeof(shared_region_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); return NULL; }
    g = (shared_region_t*)p;

    /* Initialize shared region: set mutex/cond attributes (only server does this) */
    pthread_mutexattr_t mattr;
    pthread_condattr_t  cattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
#ifdef __linux__
    /* robust can be helpful but requires careful handling on owner death; optional */
    // pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);
#endif
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    /* initialize mutexes/conds and flags */
    pthread_mutex_init(&g->req_mtx, &mattr);
    pthread_cond_init(&g->req_cond, &cattr);
    g->req_ready = 0;
    g->resp_ready = 0;
    memset(&g->rpc_slot, 0, sizeof(g->rpc_slot));

    pthread_mutex_init(&g->evt_mtx, &mattr);
    pthread_cond_init(&g->evt_cond, &cattr);
    g->evt_ring.head = g->evt_ring.tail = 0;
    g->evt_ring.cap = EVENT_RING_CAP;

    fprintf(stderr, "server_shm: ready (shared=%s)\n", SHM_NAME);

    /* main loop */
    while (1) {
        /* wait for request */
        if (pthread_mutex_lock(&g->req_mtx) != 0) { perror("pthread_mutex_lock req_mtx"); break; }
        while (!g->req_ready) {
            pthread_cond_wait(&g->req_cond, &g->req_mtx);
        }
        /* copy request */
        ipc_slot_t req;
        memcpy(&req, &g->rpc_slot, sizeof(req));
        /* reset request flag */
        g->req_ready = 0;
        pthread_mutex_unlock(&g->req_mtx);

        fprintf(stderr, "server_shm: got request pid=%d op=%u seq=%u client_handle=0x%016" PRIx64 "\n",
                (int)req.pid, (unsigned)req.op, (unsigned)req.seq, req.client_handle);

        /* process it */
        ipc_slot_t resp;
        process_rpc(&req, &resp);

        /* write response and signal client */
        if (pthread_mutex_lock(&g->req_mtx) != 0) { perror("pthread_mutex_lock resp"); break; }
        memcpy(&g->rpc_slot, &resp, sizeof(resp));
        g->resp_ready = 1;
        pthread_cond_signal(&g->req_cond); /* reuse same cond for notification */
        pthread_mutex_unlock(&g->req_mtx);

        /* dispatch server-side events to the shared ring */
        dispatch_events_all();
    }

    return 0;
}

