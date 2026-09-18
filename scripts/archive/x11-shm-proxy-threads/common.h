#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <X11/Xlib.h>
/* GLX types used by client helpers/tests */
#include <GL/glx.h>

/* Request kinds */
typedef enum {
    REQ_NONE = 0,
    REQ_XOpenDisplay,
    REQ_XCloseDisplay,
    REQ_XCreateWindow,
    REQ_XMapWindow,
    REQ_XDestroyWindow,
    REQ_XInternAtom,
    REQ_XFlush,
    REQ_XSync,
    REQ_XGetWindowAttributes,
    REQ_XLockDisplay,
    REQ_XUnlockDisplay,
    REQ_XGetRealDisplay,
    REQ_XDefaultScreen,
    REQ_XRootWindow,
    REQ_XWhitePixel,
    REQ_SHUTDOWN
} req_type_t;

/* Response types (client-owned buffers) */
typedef struct {
    Display *display;
    int success;
    char server_msg[128];
} ClientDisplayResp;

typedef struct {
    Window win;
    int success;
} ClientWinResp;

typedef struct {
    int success;
} ClientSimpleResp;

/* NEW: Atom response */
typedef struct {
    Atom atom;
    int success;
} ClientAtomResp;

/* NEW: Pixel response */
typedef struct {
    unsigned long pixel;
    int success;
} ClientPixelResp;

/* NEW: generic integer response (e.g., for DefaultScreen) */
typedef struct {
    int value;
    int success;
} ClientIntResp;

/* Ring buffer config */
#define RING_CAPACITY 1024
#define RING_MASK (RING_CAPACITY - 1)
_Static_assert((RING_CAPACITY & RING_MASK) == 0, "RING_CAPACITY must be power of two");

/* Ring slot */
typedef struct {
    atomic_uint_fast64_t seq;

    uint64_t req_ticket;
    uint64_t req_id;
    req_type_t req_type;

    /* args */
    const char *display_name_ptr;
    Display *req_display_ptr; /* real Display* */

    /* XCreateWindow / XMapWindow / XDestroyWindow */
    Window req_parent;
    int req_x;
    int req_y;
    unsigned int req_width;
    unsigned int req_height;
    unsigned int req_border_width;
    int req_depth;
    unsigned int req_class;
    Visual *req_visual;
    unsigned long req_valuemask;
    XSetWindowAttributes *req_attributes;

    /* NEW: arguments for XInternAtom */
    const char *req_atom_name;
    Bool req_only_if_exists;

    /* For XGetWindowAttributes: client provides pointer here (client-owned buffer) */
    XWindowAttributes *req_out_attrs;

    void *client_resp; /* pointer to response struct */

    pthread_mutex_t wait_mutex;
    pthread_cond_t wait_cond;
    atomic_int completed;
} ring_slot_t;

/* ring */
typedef struct {
    atomic_uint_fast64_t head;
    atomic_uint_fast64_t tail;
    atomic_uint_fast64_t next_req_id;
    ring_slot_t slots[RING_CAPACITY];
} ring_t;

extern ring_t g_ring;

/* Helper: request the real Display* from a fake handle (implemented in client.c) */
Display *XGetRealDisplay(Display *fake);

/* NOTE: do not declare client-specific GLX helpers here; use real GLX symbols */

/* ring helpers */
void init_ring(ring_t *r);
ring_slot_t *ring_reserve_slot(ring_t *r);
void ring_publish_slot(ring_t *r, ring_slot_t *slot);
ring_slot_t *ring_consume_slot(ring_t *r);
void ring_release_slot(ring_t *r, ring_slot_t *slot);
void slot_wait_completed(ring_slot_t *slot);
void slot_signal_completed(ring_slot_t *slot);
int slot_timed_wait_completed(ring_slot_t *slot, long timeout_ms);

#endif /* COMMON_H */
