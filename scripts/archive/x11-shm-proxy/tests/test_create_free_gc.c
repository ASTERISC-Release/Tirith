#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifndef ZPixmap
#define ZPixmap 2
#endif

static unsigned char *make_solid_rgba(int w, int h, int *bpl_out) {
    int bpp = 32;
    int bpl = w * 4;
    size_t sz = (size_t)bpl * (size_t)h;
    unsigned char *buf = (unsigned char*)malloc(sz);
    if (!buf) return NULL;
    for (int y = 0; y < h; ++y) {
        unsigned char *row = buf + (size_t)y * bpl;
        for (int x = 0; x < w; ++x) {
            int off = x * 4;
            row[off + 0] = 0x40;  /* B */
            row[off + 1] = 0xA0;  /* G */
            row[off + 2] = 0xF0;  /* R */
            row[off + 3] = 0x00;  /* X */
        }
    }
    *bpl_out = bpl;
    return buf;
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "FAIL: XOpenDisplay\n");
        return 2;
    }

    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);

    /* Create a GC with default values (valuemask=0, values=NULL) */
    GC gc = XCreateGC(dpy, root, 0, NULL);
    if (!gc) {
        fprintf(stderr, "FAIL: XCreateGC returned NULL\n");
        XCloseDisplay(dpy);
        return 1;
    }
    printf("Created GC: %p\n", (void*)gc);

    /* Put a tiny image using that GC to ensure it’s usable */
    int w = 32, h = 32, bpl = 0;
    unsigned char *buf = make_solid_rgba(w, h, &bpl);
    if (!buf) {
        fprintf(stderr, "FAIL: alloc buffer\n");
        XFreeGC(dpy, gc);
        XCloseDisplay(dpy);
        return 2;
    }

    XImage *xi = XCreateImage(dpy, XDefaultVisual(dpy, scr), DefaultDepth(dpy, scr),
                              ZPixmap, 0, (char*)buf, w, h, 32, bpl);
    if (!xi) {
        fprintf(stderr, "FAIL: XCreateImage\n");
        free(buf);
        XFreeGC(dpy, gc);
        XCloseDisplay(dpy);
        return 2;
    }
    printf("Created XImage: %p\n", (void*)xi);

    int rc_put = XPutImage(dpy, root, gc, xi, 0, 0, 20, 20, (unsigned)w, (unsigned)h);
    XFlush(dpy);
    if (rc_put != 0) {
        fprintf(stderr, "FAIL: XPutImage rc=%d\n", rc_put);
        XDestroyImage(xi); /* frees buf */
        XFreeGC(dpy, gc);
        XCloseDisplay(dpy);
        return 1;
    }
    printf("XPutImage succeeded\n");

    /* Free GC once (should succeed) */
    int rc_free1 = XFreeGC(dpy, gc);
    if (rc_free1 != 0) {
        fprintf(stderr, "FAIL: XFreeGC first call rc=%d\n", rc_free1);
        XDestroyImage(xi);
        XCloseDisplay(dpy);
        return 1;
    }
    printf("XFreeGC first call succeeded\n");

    /* Free GC again (should fail in our wrapper and return non-zero) */
    int rc_free2 = XFreeGC(dpy, gc);
    if (rc_free2 == 0) {
        fprintf(stderr, "FAIL: XFreeGC second call unexpectedly succeeded\n");
        XDestroyImage(xi);
        XCloseDisplay(dpy);
        return 1;
    }

    // Add: freeing a bogus handle should fail
    GC bogus = (GC)(uintptr_t)0x1;
    int rc_bogus = XFreeGC(dpy, bogus);
    if (rc_bogus == 0) {
        fprintf(stderr, "FAIL: XFreeGC bogus handle unexpectedly succeeded\n");
        XDestroyImage(xi);
        XCloseDisplay(dpy);
        return 1;
    }

    XDestroyImage(xi); /* also frees buf */
    XCloseDisplay(dpy);

    fprintf(stderr, "PASS: XCreateGC/XFreeGC basic functionality\n");
    return 0;
}