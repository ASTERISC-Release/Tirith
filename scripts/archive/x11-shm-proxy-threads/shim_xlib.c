/* LD_PRELOAD shim: intercept selected Xlib functions and forward them to server.
 * Build as: gcc -shared -fPIC -o libx11shim.so shim_xlib.c -ldl -lpthread
 */
#define _GNU_SOURCE
#include "common.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

/* Helpers to publish requests similar to client.c wrappers */
static void publish_simple(req_type_t t, Display *dpy) {
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = t;
    slot->req_display_ptr = dpy;
    ClientSimpleResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    slot->client_resp = NULL;
}

/* Intercepted functions */
int XFlush(Display *dpy) {
    publish_simple(REQ_XFlush, dpy);
    return 1;
}

int XSync(Display *dpy, Bool discard) {
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XSync;
    slot->req_display_ptr = dpy;
    slot->req_only_if_exists = discard;
    ClientSimpleResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    slot->client_resp = NULL;
    return resp.success;
}

void XLockDisplay(Display *dpy) {
    publish_simple(REQ_XLockDisplay, dpy);
}

void XUnlockDisplay(Display *dpy) {
    publish_simple(REQ_XUnlockDisplay, dpy);
}

/* XCloseDisplay: forward and return success */
int XCloseDisplay(Display *dpy) {
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XCloseDisplay;
    slot->req_display_ptr = dpy;
    ClientSimpleResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    int ok = resp.success;
    slot->client_resp = NULL;
    return ok;
}

Window XRootWindow(Display *dpy, int screen) {
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XRootWindow;
    slot->req_display_ptr = dpy;
    slot->req_x = screen;
    ClientWinResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    Window w = resp.win;
    slot->client_resp = NULL;
    return w;
}

unsigned long XWhitePixel(Display *dpy, int screen) {
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

Atom XInternAtom(Display *dpy, const char *name, Bool only_if_exists) {
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XInternAtom;
    slot->req_display_ptr = dpy;
    slot->req_atom_name = name;
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

Status XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *out) {
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XGetWindowAttributes;
    slot->req_display_ptr = dpy;
    slot->req_parent = w;
    slot->req_out_attrs = out;
    ClientSimpleResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    int ok = resp.success;
    slot->client_resp = NULL;
    return ok ? (Status)1 : (Status)0;
}

/* XCreateWindow / XMapWindow / XDestroyWindow forwarded similarly */
Window XCreateWindow(Display *dpy, Window parent, int x, int y,
                     unsigned int width, unsigned int height,
                     unsigned int border_width, int depth,
                     unsigned int klass, Visual *visual,
                     unsigned long valuemask, XSetWindowAttributes *attributes) {
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XCreateWindow;
    slot->req_display_ptr = dpy;
    slot->req_parent = parent;
    slot->req_x = x; slot->req_y = y;
    slot->req_width = width; slot->req_height = height;
    slot->req_border_width = border_width; slot->req_depth = depth;
    slot->req_class = klass; slot->req_visual = visual;
    slot->req_valuemask = valuemask; slot->req_attributes = attributes;
    ClientWinResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    Window w = resp.win;
    slot->client_resp = NULL;
    return w;
}

int XMapWindow(Display *dpy, Window w) {
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

int XDestroyWindow(Display *dpy, Window w) {
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_XDestroyWindow;
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
