#define _GNU_SOURCE
#include "common.h"
#include "real_x11.h"
#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* minimal X error handler flag used for close; single-consumer server */
static atomic_int g_xlib_error_flag = 0;
static int xlib_error_handler_for_close(Display *d, XErrorEvent *ev) {
    (void)d; (void)ev;
    atomic_store_explicit(&g_xlib_error_flag, 1, memory_order_release);
    return 0;
}

/* (mapping helpers implemented below) */

/* server-owned ID table mapping small integer IDs to real Display*.
   This is simpler to marshal across processes: the client receives a small
   integer encoded as a pointer-sized value. IDs start at 1. For this
   prototype we use a fixed-capacity table and a mutex. */
#define DPY_TABLE_CAP 256
struct dpy_table_entry { Display *real; };
static struct dpy_table_entry dpy_table[DPY_TABLE_CAP];
static atomic_uint_fast32_t dpy_next_id = 1; /* id 0 reserved */
static pthread_mutex_t dpy_table_lock = PTHREAD_MUTEX_INITIALIZER;

/* create a handle: allocate an id and store real Display* in table */
static void *create_handle(Display *real) {
    if (!real) return NULL;
    uint32_t id = (uint32_t)atomic_fetch_add_explicit(&dpy_next_id, 1, memory_order_acq_rel);
    /* wrap-around protection: if id==0 or >capacity, search for free slot */
    uint32_t start = id % DPY_TABLE_CAP;
    pthread_mutex_lock(&dpy_table_lock);
    for (uint32_t i = 0; i < DPY_TABLE_CAP; ++i) {
        uint32_t idx = (start + i) % DPY_TABLE_CAP;
        if (dpy_table[idx].real == NULL) {
            dpy_table[idx].real = real;
            pthread_mutex_unlock(&dpy_table_lock);
            /* encode handle as (uintptr_t)(idx+1) to avoid 0 */
            return (void *)(uintptr_t)(idx + 1);
        }
    }
    pthread_mutex_unlock(&dpy_table_lock);
    return NULL; /* table full */
}

/* lookup: decode handle (idx+1) -> Display* */
static Display *lookup_handle(void *handle) {
    if (!handle) return NULL;
    uintptr_t v = (uintptr_t)handle;
    if (v == 0) return NULL;
    uint32_t idx = (uint32_t)(v - 1);
    if (idx >= DPY_TABLE_CAP) return NULL;
    pthread_mutex_lock(&dpy_table_lock);
    Display *r = dpy_table[idx].real;
    pthread_mutex_unlock(&dpy_table_lock);
    return r;
}

/* remove mapping and return the real Display* */
static Display *remove_handle(void *handle) {
    if (!handle) return NULL;
    uintptr_t v = (uintptr_t)handle;
    if (v == 0) return NULL;
    uint32_t idx = (uint32_t)(v - 1);
    if (idx >= DPY_TABLE_CAP) return NULL;
    pthread_mutex_lock(&dpy_table_lock);
    Display *r = dpy_table[idx].real;
    dpy_table[idx].real = NULL;
    pthread_mutex_unlock(&dpy_table_lock);
    return r;
}

void *server_thread_fn(void *arg) {
    (void)arg;

    /* resolve real symbols once (real_x11 does pthread_once). If missing -> exit */
    XOpenDisplay_fn_t real_open = get_real_XOpenDisplay();
    XCloseDisplay_fn_t real_close = get_real_XCloseDisplay();
    XCreateWindow_fn_t real_create = get_real_XCreateWindow();
    XMapWindow_fn_t real_map = get_real_XMapWindow();
    XDestroyWindow_fn_t real_destroy = get_real_XDestroyWindow();
    XInternAtom_fn_t real_intern = get_real_XInternAtom();
    XFlush_fn_t real_flush = get_real_XFlush();
    XSync_fn_t real_sync = get_real_XSync();
    XGetWindowAttributes_fn_t real_getwinattrs = get_real_XGetWindowAttributes();
    XLockDisplay_fn_t real_lock = get_real_XLockDisplay();
    XUnlockDisplay_fn_t real_unlock = get_real_XUnlockDisplay();

    if (!real_open || !real_close) {
        /* required symbols must exist; abort early */
        fprintf(stderr, "[server] fatal: required real_x11 symbols missing; exiting\n");
        exit(1);
    }

    for (;;) {
        ring_slot_t *slot = ring_consume_slot(&g_ring);
        if (!slot) continue;

        req_type_t req = slot->req_type;

        if (req == REQ_SHUTDOWN) {
            if (slot->client_resp) {
                ClientDisplayResp *r = (ClientDisplayResp*)slot->client_resp;
                r->display = NULL;
                r->success = 1;
            }
            slot_signal_completed(slot);
            ring_release_slot(&g_ring, slot);
            break;
        }

        switch (req) {
        case REQ_XOpenDisplay: {
            const char *name = slot->display_name_ptr;
            Display *real_dpy = real_open(name && name[0] ? name : NULL);
            if (slot->client_resp) {
                ClientDisplayResp *r = (ClientDisplayResp*)slot->client_resp;
                void *fake = NULL;
                if (real_dpy) fake = create_handle(real_dpy);
                r->display = fake;
                r->success = (real_dpy && fake) ? 1 : 0;
            } else {
                if (real_dpy) real_close(real_dpy);
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XCloseDisplay: {
            fprintf(stderr, "[server] REQ_XCloseDisplay: entering (fake_dpy=%p)\n",
                    (void *)slot->req_display_ptr);
            int ok = 0;
            void *fake = slot->req_display_ptr;
            /* remove mapping and get real Display* */
            Display *d = remove_handle(fake);
            if (d) {
                ok = real_close(d);
            }
            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp*)slot->client_resp;
                r->success = ok ? 1 : 0;
            }
            fprintf(stderr, "[server] REQ_XCloseDisplay: done (fake_dpy=%p) ok=%d\n",
                    (void *)fake, ok);
            slot_signal_completed(slot);
            break;
        }

        case REQ_XCreateWindow: {
            Window win = 0;
            Display *d = lookup_handle(slot->req_display_ptr);
            if (real_create && d) {
                win = real_create(d,
                                  slot->req_parent,
                                  slot->req_x, slot->req_y,
                                  slot->req_width, slot->req_height,
                                  slot->req_border_width,
                                  slot->req_depth,
                                  slot->req_class,
                                  slot->req_visual,
                                  slot->req_valuemask,
                                  slot->req_attributes);
            }
            if (slot->client_resp) {
                ClientWinResp *r = (ClientWinResp*)slot->client_resp;
                r->win = win;
                r->success = (win ? 1 : 0);
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XMapWindow: {
            int ok = 0;
            Display *d = lookup_handle(slot->req_display_ptr);
            if (real_map && d) {
                Window w = slot->req_parent;
                ok = real_map(d, w);
            }
            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp*)slot->client_resp;
                r->success = ok ? 1 : 0;
            }
            slot_signal_completed(slot);
            break;
        }

    case REQ_XDestroyWindow: {
        int ok = 0;
        void *fake = slot->req_display_ptr;
        Display *d = lookup_handle(fake);
        /* Lightweight entry log */
        fprintf(stderr, "[server] REQ_XDestroyWindow: entering (fake_dpy=%p, win=0x%lx)\n",
            (void *)fake, (unsigned long)slot->req_parent);

        if (real_destroy && d) {
        Window w = slot->req_parent;
        real_destroy(d, w);
        ok = 1; /* treat as success; if you want X error detection add XSync+handler */
        }

        /* Lightweight exit log */
        fprintf(stderr, "[server] REQ_XDestroyWindow: done (fake_dpy=%p, win=0x%lx) ok=%d\n",
            (void *)fake, (unsigned long)slot->req_parent, ok);

            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp *)slot->client_resp;
                r->success = ok ? 1 : 0;
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XInternAtom: {
            Atom a = None;
            Display *d = lookup_handle(slot->req_display_ptr);
            if (real_intern && d && slot->req_atom_name) {
                a = real_intern(d, slot->req_atom_name, slot->req_only_if_exists);
            }
            if (slot->client_resp) {
                ClientAtomResp *r = (ClientAtomResp *)slot->client_resp;
                r->atom = a;
                r->success = (a != None) ? 1 : 0;
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XRootWindow: {
            Window w = 0;
            Display *d = lookup_handle(slot->req_display_ptr);
            int screen = slot->req_x;
            if (d) {
                w = DefaultRootWindow(d);
            }
            if (slot->client_resp) {
                ClientWinResp *r = (ClientWinResp*)slot->client_resp;
                r->win = w;
                r->success = (w ? 1 : 0);
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XGetRealDisplay: {
            /* return the real Display* for this fake handle */
            Display *real = lookup_handle(slot->req_display_ptr);
            if (slot->client_resp) {
                ClientDisplayResp *r = (ClientDisplayResp*)slot->client_resp;
                r->display = real;
                r->success = (real ? 1 : 0);
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XDefaultScreen: {
            int scr = -1;
            Display *d = lookup_handle(slot->req_display_ptr);
            if (d) {
                /* use the real DefaultScreen on the server-owned Display */
                scr = DefaultScreen(d);
            }
            if (slot->client_resp) {
                ClientIntResp *r = (ClientIntResp*)slot->client_resp;
                r->value = scr;
                r->success = (scr >= 0) ? 1 : 0;
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XWhitePixel: {
            unsigned long pixel = 0;
            Display *d = lookup_handle(slot->req_display_ptr);
            int screen = slot->req_x;
            if (d) {
                pixel = WhitePixel(d, screen);
            }
            if (slot->client_resp) {
                ClientPixelResp *r = (ClientPixelResp*)slot->client_resp;
                r->pixel = pixel;
                r->success = 1;
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XLockDisplay: {
            Display *d = lookup_handle(slot->req_display_ptr);
            if (real_lock && d) {
                real_lock(d);
            }
            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp*)slot->client_resp;
                r->success = 1;
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XUnlockDisplay: {
            Display *d = lookup_handle(slot->req_display_ptr);
            if (real_unlock && d) {
                real_unlock(d);
            }
            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp*)slot->client_resp;
                r->success = 1;
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XFlush: {
            int ok = 0;
            Display *d = lookup_handle(slot->req_display_ptr);
            if (real_flush && d) {
                /* XFlush is void; call and treat as success */
                real_flush(d);
                ok = 1;
            }
            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp*)slot->client_resp;
                r->success = ok;
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XSync: {
            int ok = 0;
            Display *d = lookup_handle(slot->req_display_ptr);
            if (real_sync && d) {
                /* XSync returns int (0) in some impls; treat as success */
                real_sync(d, (Bool)slot->req_only_if_exists);
                ok = 1;
            }
            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp*)slot->client_resp;
                r->success = ok;
            }
            slot_signal_completed(slot);
            break;
        }

        case REQ_XGetWindowAttributes: {
            Status st = 0;
            Display *d = lookup_handle(slot->req_display_ptr);
            if (real_getwinattrs && d && slot->req_out_attrs) {
                /* server writes directly into client-owned memory */
                st = real_getwinattrs(d, slot->req_parent, slot->req_out_attrs);
            }
            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp*)slot->client_resp;
                r->success = (st != 0) ? 1 : 0;
            }
            slot_signal_completed(slot);
            break;
        }

        default:
            if (slot->client_resp) {
                ClientSimpleResp *r = (ClientSimpleResp*)slot->client_resp;
                r->success = 0;
            }
            slot_signal_completed(slot);
            break;
        }

        ring_release_slot(&g_ring, slot);
    }

    return NULL;
}
