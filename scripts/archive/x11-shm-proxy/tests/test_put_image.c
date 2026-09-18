/* test_put_image.c
 * compile: gcc -o test_put_image test_put_image.c -lX11
 * run: LD_PRELOAD=./libx11ipc.so ./test_put_image
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "XOpenDisplay failed\n"); return 1; }

    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int w = 256, h = 256;

    /* simple 24/32 bpp ZPixmap buffer: 4 bytes per pixel (RGBA or XRGB) */
    int bpp = 32;
    int bytes_per_line = (w * bpp + 7) / 8;
    size_t bufsize = bytes_per_line * h;
    char *buf = malloc(bufsize);
    if (!buf) { XCloseDisplay(dpy); return 1; }

    /* fill with gradient */
    for (int y = 0; y < h; ++y) {
        unsigned char *row = (unsigned char*)(buf + y * bytes_per_line);
        for (int x = 0; x < w; ++x) {
            unsigned char r = (unsigned char)((x * 255) / (w - 1));
            unsigned char g = (unsigned char)((y * 255) / (h - 1));
            unsigned char b = 0x80;
            /* assuming little-endian XImage with 32bpp XRGB: B G R _ */
            int off = x * 4;
            row[off + 0] = b; /* blue */
            row[off + 1] = g; /* green */
            row[off + 2] = r; /* red */
            row[off + 3] = 0; /* padding */
        }
    }

    /* create XImage client-side */
    XImage *xi = XCreateImage(dpy, DefaultVisual(dpy, scr), DefaultDepth(dpy, scr),
                              ZPixmap, 0, buf, w, h, 32, bytes_per_line);
    if (!xi) {
        fprintf(stderr, "XCreateImage failed\n");
        free(buf);
        XCloseDisplay(dpy);
        return 1;
    }

    /* Put to root window at 100,100 */
    int rc = XPutImage(dpy, root, DefaultGC(dpy, scr), xi, 0, 0, 100, 100, w, h);
    if (rc != Success) {
        fprintf(stderr, "XPutImage wrapper returned error %d\n", rc);
    } else {
        fprintf(stderr, "XPutImage wrapper succeeded\n");
    }

    /* note: XDestroyImage will free xi->data; but in our client wrapper we usually
       destroyed it manually. For the test free buf if XDestroyImage doesn't free it. */
    XDestroyImage(xi);
    XCloseDisplay(dpy);
    return 0;
}

