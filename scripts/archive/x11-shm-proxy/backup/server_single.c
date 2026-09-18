/*
 server_single.c
 Single-client X11 IPC server. Server opens its own X Display(s) on request
 and services RPCs coming from the client. Uses single RPC slot in shared memory
 and one event ring for delivering XEvents to the client.

 Build:
   gcc -g -O0 -Wall -Wextra -o server_single server_single.c -lX11 -pthread -lrt
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>

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

/* IPC slot layout */
typedef struct {
    uint32_t op;
    pid_t pid;
    uint32_t seq;
    uint64_t client_handle; /* client-sent local Display* (as integer) */
    union {
        struct { char name[240]; } open_display;
        struct { uint64_t handle; } close_display;
        struct { int x,y,w,h,border; unsigned long border_px, bg; } create_window;
        struct { uint64_t window_handle; } simple_window;
    } p;
    int32_t ret_int;
    uint64_t ret_handle; /* server-side opaque handles or Window values */
    int8_t status;
} ipc_slot_t;

/* event ring */
typedef struct {
    uint32_t head;
    uint32_t tail;
    uint32_t cap;
    XEvent events[EVENT_RING_CAP];
} event_ring_t;

/* server maps client_handle -> server Display* */
typedef struct DisplayEntry {
    uint64_t client_handle; /* the client's Display* as integer */
    Display *dpy;
    struct DisplayEntry *next;
} DisplayEntry;

/* window map (server-side Window -> opaque id if desired). We'll return Window as-is. */
typedef struct WindowEntry {
    uint64_t id; /* opaque id if needed */
    Window w;
    Display *dpy;
    struct WindowEntry *next;
} WindowEntry;

/* globals */
static ipc_slot_t *rpc_slot = NULL;
static sem_t *req_sem = NULL;
static sem_t *resp_sem = NULL;

static event_ring_t *evt_ring = NULL;
static sem_t *evt_sem = NULL;
static sem_t *evt_mtx = NULL;

static DisplayEntry *display_map = NULL;
static WindowEntry *window_map = NULL;
static pthread_mutex_t maps_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t next_handle = 1;
static uint64_t alloc_handle() { return __sync_fetch_and_add(&next_handle, 1); }

/* helper lookup */
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

/* event push: write event into ring; return 0 on success */
static int push_event(event_ring_t *ring, sem_t *sem, sem_t *mtx, XEvent *ev) {
    if (!ring || !sem || !mtx) return -1;
    if (sem_wait(mtx) != 0) return -1;
    uint32_t next_tail = (ring->tail + 1) % ring->cap;
    if (next_tail == ring->head) { sem_post(mtx); return -1; } /* full */
    ring->events[ring->tail] = *ev;
    ring->tail = next_tail;
    sem_post(mtx);
    sem_post(sem);
    return 0;
}

/* process RPC request (fills resp) */
static void process_rpc(const ipc_slot_t *req, ipc_slot_t *resp) {
    memset(resp, 0, sizeof(*resp));
    resp->op = req->op; resp->pid = req->pid; resp->seq = req->seq; resp->status = 1;

    switch (req->op) {
    case OP_XOpenDisplay: {
        const char *name = req->p.open_display.name;
        Display *dpy = XOpenDisplay(name[0] ? name : NULL);
        if (!dpy) { resp->status = 1; resp->ret_handle = 0; }
        else {
            uint64_t hid = alloc_handle();
            map_server_display(req->client_handle, dpy);
            resp->status = 0;
            resp->ret_handle = hid;
        }
        break;
    }
    case OP_XCloseDisplay: {
        /* close server display mapped to this client_handle */
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
                                       req->p.create_window.x, req->p.create_window.y,
                                       req->p.create_window.w, req->p.create_window.h,
                                       req->p.create_window.border,
                                       req->p.create_window.border_px,
                                       req->p.create_window.bg);
        /* we return the server Window id directly (Window is typically an integer type) */
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
    default:
        resp->status = 1;
        break;
    }
}

/* dispatch events from all server displays: grab pending events and push to ring */
static void dispatch_events(void) {
    pthread_mutex_lock(&maps_lock);
    for (DisplayEntry *e = display_map; e; e = e->next) {
        Display *dpy = e->dpy;
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            /* zero the display pointer inside the event so the client can set it to its local Display* */
            ev.xany.display = NULL;
            if (push_event(evt_ring, evt_sem, evt_mtx, &ev) != 0) {
                fprintf(stderr, "server: event ring full; dropping event\n");
            }
        }
    }
    pthread_mutex_unlock(&maps_lock);
}

int main(void) {
    /* setup RPC shm */
    int fd = shm_open(RPC_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) { perror("shm_open rpc"); return 1; }
    if (ftruncate(fd, sizeof(ipc_slot_t)) != 0) { perror("ftruncate rpc"); return 1; }
    rpc_slot = mmap(NULL, sizeof(ipc_slot_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (rpc_slot == MAP_FAILED) { perror("mmap rpc"); return 1; }
    memset(rpc_slot, 0, sizeof(ipc_slot_t));

    req_sem = sem_open(REQ_SEM_NAME, O_CREAT, 0666, 0);
    if (req_sem == SEM_FAILED) { perror("sem_open req"); return 1; }
    resp_sem = sem_open(RESP_SEM_NAME, O_CREAT, 0666, 0);
    if (resp_sem == SEM_FAILED) { perror("sem_open resp"); return 1; }

    /* setup event ring */
    int efd = shm_open(EVENT_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (efd < 0) { perror("shm_open evt"); return 1; }
    if (ftruncate(efd, sizeof(event_ring_t)) != 0) { perror("ftruncate evt"); return 1; }
    evt_ring = mmap(NULL, sizeof(event_ring_t), PROT_READ|PROT_WRITE, MAP_SHARED, efd, 0);
    if (evt_ring == MAP_FAILED) { perror("mmap evt"); return 1; }
    evt_ring->head = evt_ring->tail = 0; evt_ring->cap = EVENT_RING_CAP;

    evt_sem = sem_open(EVENT_SEM_NAME, O_CREAT, 0666, 0);
    if (evt_sem == SEM_FAILED) { perror("sem_open evt"); return 1; }
    evt_mtx = sem_open(EVENT_MTX_NAME, O_CREAT, 0666, 1);
    if (evt_mtx == SEM_FAILED) { perror("sem_open evt_mtx"); return 1; }

    fprintf(stderr, "server_single: ready\n");

    /* main loop: service RPCs then dispatch events */
    while (1) {
        /* wait for a request */
        if (sem_wait(req_sem) != 0) {
            if (errno == EINTR) continue;
            perror("sem_wait req");
            break;
        }

        /* log and copy request */
        if (rpc_slot) {
            fprintf(stderr, "server_single: got request pid=%d op=%u seq=%u client_handle=0x%016" PRIx64 "\n",
                    (int)rpc_slot->pid, (unsigned)rpc_slot->op, (unsigned)rpc_slot->seq, rpc_slot->client_handle);
        }
        ipc_slot_t req;
        memcpy(&req, rpc_slot, sizeof(req));
        ipc_slot_t resp;
        process_rpc(&req, &resp);
        /* write response and signal */
        memcpy(rpc_slot, &resp, sizeof(resp));
        sem_post(resp_sem);

        /* after handling RPC, process any pending server-side events */
        dispatch_events();
    }

    return 0;
}

