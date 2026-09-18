#define _GNU_SOURCE
#include "common.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <GL/glx.h>
#include "real_x11.h"

/* debug: enable by setting XPROXY_DEBUG=1 in the environment */
static int xproxy_debug_enabled = -1;
static void proxy_debug(const char *fmt, ...) {
    if (xproxy_debug_enabled == -1) {
        const char *e = getenv("XPROXY_DEBUG");
        xproxy_debug_enabled = (e && e[0]) ? 1 : 0;
    }
    if (!xproxy_debug_enabled) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* client-side reverse mapping: real Display* -> fake handle */
#define CLIENT_DPY_TABLE_CAP 128
static struct {
    Display *real;
    void *fake;
} client_dpy_table[CLIENT_DPY_TABLE_CAP];
static pthread_mutex_t client_dpy_lock = PTHREAD_MUTEX_INITIALIZER;

static void client_register_mapping(void *fake, Display *real) {
    pthread_mutex_lock(&client_dpy_lock);
    for (int i = 0; i < CLIENT_DPY_TABLE_CAP; ++i) {
        if (client_dpy_table[i].real == NULL) {
            client_dpy_table[i].real = real;
            client_dpy_table[i].fake = fake;
            break;
        }
    }
    pthread_mutex_unlock(&client_dpy_lock);
}

static void client_remove_mapping(void *fake) {
    pthread_mutex_lock(&client_dpy_lock);
    for (int i = 0; i < CLIENT_DPY_TABLE_CAP; ++i) {
        if (client_dpy_table[i].fake == fake) {
            client_dpy_table[i].real = NULL;
            client_dpy_table[i].fake = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&client_dpy_lock);
}

static void *client_lookup_fake(Display *real) {
    if (!real) return NULL;
    pthread_mutex_lock(&client_dpy_lock);
    void *f = NULL;
    for (int i = 0; i < CLIENT_DPY_TABLE_CAP; ++i) {
        if (client_dpy_table[i].real == real) {
            f = client_dpy_table[i].fake;
            break;
        }
    }
    pthread_mutex_unlock(&client_dpy_lock);
    return f;
}

/* Heuristic: fake handles are encoded as small integer values (idx+1).
   Treat any pointer value <= FAKE_PTR_MAX as a fake handle. This keeps the
   client able to forward when tests use the fake display directly. */
#define FAKE_PTR_MAX 0x10000
static int is_fake_display_ptr(Display *dpy) {
    uintptr_t v = (uintptr_t)dpy;
    return (v != 0 && v <= (uintptr_t)FAKE_PTR_MAX);
}

/* XOpenDisplay */
Display *XOpenDisplay(const char *display_name) {
    proxy_debug("[client] XOpenDisplay name='%s'\n", display_name ? display_name : "(null)");
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XOpenDisplay;
    slot->display_name_ptr = display_name;
    ClientDisplayResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    Display *d = resp.display;
    slot->client_resp = NULL;
    return d;
}

/* XCloseDisplay */
int XCloseDisplay(Display *display) {
    proxy_debug("[client] XCloseDisplay fake=%p\n", (void*)display);
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XCloseDisplay;
    slot->req_display_ptr = display;
    ClientSimpleResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    int ok = resp.success;
    slot->client_resp = NULL;
    return ok;
}

/* XCreateWindow */
Window XCreateWindow(Display *dpy,
                     Window parent,
                     int x, int y, unsigned int width, unsigned int height,
                     unsigned int border_width, int depth,
                     unsigned int klass,
                     Visual *visual,
                     unsigned long valuemask,
                     XSetWindowAttributes *attributes)
{
    proxy_debug("[client] XCreateWindow dpy=%p parent=0x%lx\n", (void*)dpy, (unsigned long)parent);
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XCreateWindow;
    slot->req_display_ptr = dpy;
    slot->req_parent = parent;
    slot->req_x = x;
    slot->req_y = y;
    slot->req_width = width;
    slot->req_height = height;
    slot->req_border_width = border_width;
    slot->req_depth = depth;
    slot->req_class = klass;
    slot->req_visual = visual;
    slot->req_valuemask = valuemask;
    slot->req_attributes = attributes;
    ClientWinResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    Window w = resp.win;
    slot->client_resp = NULL;
    return w;
}

/* XMapWindow */
int XMapWindow(Display *dpy, Window w) {
    proxy_debug("[client] XMapWindow dpy=%p win=0x%lx\n", (void*)dpy, (unsigned long)w);
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XMapWindow;
    slot->req_display_ptr = dpy;
    slot->req_parent = w;
    ClientSimpleResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    int ok = resp.success;
    slot->client_resp = NULL;
    return ok;
}

/* XDestroyWindow */
int XDestroyWindow(Display *dpy, Window w) {
    proxy_debug("[client] XDestroyWindow dpy=%p win=0x%lx\n", (void*)dpy, (unsigned long)w);
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XDestroyWindow;
    slot->req_display_ptr = dpy;
    slot->req_parent = w;
    ClientSimpleResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);

    /* use a 5 second timeout to avoid hanging forever */
    const long timeout_ms = 5000;
    int wait_rc = slot_timed_wait_completed(slot, timeout_ms);
    if (wait_rc != 0) {
        fprintf(stderr, "[client] ERROR: XDestroyWindow timed out after %ld ms for window 0x%lx (dpy=%p)\n",
                timeout_ms, (unsigned long)w, (void*)dpy);
        /* best-effort: clear the client_resp to avoid stale pointer, return failure */
        slot->client_resp = NULL;
        return 0;
    }

    int ok = resp.success;
    slot->client_resp = NULL;
    return ok;
}


/* XInternAtom */
Atom XInternAtom(Display *dpy, const char *atom_name, Bool only_if_exists) {
    proxy_debug("[client] XInternAtom dpy=%p name=%s\n", (void*)dpy, atom_name ? atom_name : "(null)");
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XInternAtom;
    slot->req_display_ptr = dpy;
    slot->req_atom_name = atom_name;
    slot->req_only_if_exists = only_if_exists;
    ClientAtomResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    Atom a = resp.atom;
    slot->client_resp = NULL;
    return a;
}

/* XGetRealDisplay - request server to return real Display* for a fake handle */
Display *XGetRealDisplay(Display *fake) {
    proxy_debug("[client] XGetRealDisplay(fake=%p)\n", (void*)fake);
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XGetRealDisplay;
    slot->req_display_ptr = fake;
    ClientDisplayResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    Display *real = resp.display;
    if (real && fake) client_register_mapping(fake, real);
    slot->client_resp = NULL;
    return real;
}

/* XRootWindow */
Window XRootWindow(Display *dpy, int screen) {
    proxy_debug("[client] XRootWindow dpy=%p screen=%d\n", (void*)dpy, screen);
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XRootWindow;
    slot->req_display_ptr = dpy;
    slot->req_x = screen; /* reuse req_x for screen number */
    ClientWinResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    Window w = resp.win;
    slot->client_resp = NULL;
    return w;
}

/* XWhitePixel (wrapper) */
unsigned long XWhitePixel(Display *dpy, int screen) {
    proxy_debug("[client] XWhitePixel dpy=%p screen=%d\n", (void*)dpy, screen);
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XWhitePixel;
    slot->req_display_ptr = dpy;
    slot->req_x = screen;
    ClientPixelResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    unsigned long p = resp.pixel;
    slot->client_resp = NULL;
    return p;
}

/* XFlush */
int XFlush(Display *dpy) {
    proxy_debug("[client] XFlush dpy=%p\n", (void*)dpy);
    /* If caller passed a fake handle directly, forward it. Otherwise try
       to map a real Display* to a fake returned earlier by XGetRealDisplay. */
    void *fake = NULL;
    if (is_fake_display_ptr(dpy)) {
        fake = (void*)dpy;
    } else {
        fake = client_lookup_fake(dpy);
    }
    if (fake) {
        ring_slot_t *slot = ring_reserve_slot(&g_ring);
        slot->req_type = REQ_XFlush;
        slot->req_display_ptr = fake;
        ClientSimpleResp resp;
        memset(&resp, 0, sizeof(resp));
        slot->client_resp = &resp;
        ring_publish_slot(&g_ring, slot);
        slot_wait_completed(slot);
        int ok = resp.success;
        slot->client_resp = NULL;
        return ok;
    } else {
        proxy_debug("[client] XFlush -> calling real locally dpy=%p\n", (void*)dpy);
        XFlush_fn_t real_flush = get_real_XFlush();
        if (real_flush) {
            real_flush(dpy);
            return 1;
        }
        return 0;
    }
}

/* XSync */
int XSync(Display *dpy, Bool discard) {
    proxy_debug("[client] XSync dpy=%p discard=%d\n", (void*)dpy, (int)discard);
    void *fake = NULL;
    if (is_fake_display_ptr(dpy)) {
        fake = (void*)dpy;
    } else {
        fake = client_lookup_fake(dpy);
    }
    if (fake) {
        ring_slot_t *slot = ring_reserve_slot(&g_ring);
        slot->req_type = REQ_XSync;
        slot->req_display_ptr = fake;
        slot->req_only_if_exists = discard; /* reuse flag */
        ClientSimpleResp resp;
        memset(&resp, 0, sizeof(resp));
        slot->client_resp = &resp;
        ring_publish_slot(&g_ring, slot);
        slot_wait_completed(slot);
        int ok = resp.success;
        slot->client_resp = NULL;
        return ok;
    } else {
        proxy_debug("[client] XSync -> calling real locally dpy=%p\n", (void*)dpy);
        XSync_fn_t real_sync = get_real_XSync();
        if (real_sync) return real_sync(dpy, discard);
        return 0;
    }
}

/* XGetWindowAttributes */
Status XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *out_attrs) {
    proxy_debug("[client] XGetWindowAttributes dpy=%p win=0x%lx\n", (void*)dpy, (unsigned long)w);
    void *fake = NULL;
    if (is_fake_display_ptr(dpy)) fake = (void*)dpy; else fake = client_lookup_fake(dpy);
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XGetWindowAttributes;
    slot->req_display_ptr = fake ? fake : dpy;
    slot->req_parent = w;
    slot->req_out_attrs = out_attrs; /* client-allocated buffer where server writes */
    ClientSimpleResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    int ok = resp.success;
    slot->client_resp = NULL;
    return ok ? (Status)1 : (Status)0;
}

/* XDefaultScreen - return default screen index for a Display.
   If the display is a fake handle (or maps to a fake), forward to server.
   Otherwise, call the real XDefaultScreen locally. */
int XDefaultScreen(Display *display) {
    proxy_debug("[client] XDefaultScreen dpy=%p\n", (void*)display);
    void *fake = NULL;
    if (is_fake_display_ptr(display)) fake = (void*)display; else fake = client_lookup_fake(display);
    if (fake) {
        ring_slot_t *slot = ring_reserve_slot(&g_ring);
        slot->req_type = REQ_XDefaultScreen;
        slot->req_display_ptr = fake;
        ClientIntResp resp;
        memset(&resp, 0, sizeof(resp));
        slot->client_resp = &resp;
        ring_publish_slot(&g_ring, slot);
        slot_wait_completed(slot);
        int v = resp.value;
        slot->client_resp = NULL;
        return v;
    } else {
        proxy_debug("[client] XDefaultScreen -> calling real locally dpy=%p\n", (void*)display);
        XDefaultScreen_fn_t real_def = get_real_XDefaultScreen();
        if (real_def) return real_def(display);
        return 0;
    }
}

/* No client-specific GLX helpers: tests will use real GLX symbols directly
   after resolving a real Display* with XGetRealDisplay. */
