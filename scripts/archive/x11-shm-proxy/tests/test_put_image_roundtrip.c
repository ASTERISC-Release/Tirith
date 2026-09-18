/* test_put_image_roundtrip.c
 * Compile: gcc -o test_put_image_roundtrip test_put_image_roundtrip.c -lX11
 * Run: LD_PRELOAD=./libx11ipc.so ./test_put_image_roundtrip
 *
 * Exits 0 on PASS, non-zero on FAILURE.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* small helper to create a simple 32bpp gradient buffer */
static unsigned char *make_gradient(int w, int h, int *out_bpl, int *out_bpp) {
    int bpp = 32;
    int bytes_per_line = ((w * bpp + 31)/32) * 4; /* align to 32-bit pad */
    size_t size = (size_t)bytes_per_line * (size_t)h;
    unsigned char *buf = malloc(size);
    if (!buf) return NULL;
    for (int y = 0; y < h; ++y) {
        unsigned char *row = buf + (size_t)y * bytes_per_line;
        for (int x = 0; x < w; ++x) {
            unsigned char r = (unsigned char)((x*255)/(w-1));
            unsigned char g = (unsigned char)((y*255)/(h-1));
            unsigned char b = 0x80;
            int off = x * 4;
            row[off + 0] = b;
            row[off + 1] = g;
            row[off + 2] = r;
            row[off + 3] = 0;
        }
    }
    *out_bpl = bytes_per_line;
    *out_bpp = bpp;
    return buf;
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "XOpenDisplay failed\n"); return 2; }

    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);

    /* small test image */
    int w = 128, h = 128;
    int bytes_per_line = 0, bits_per_pixel = 0;
    unsigned char *buf = make_gradient(w, h, &bytes_per_line, &bits_per_pixel);
    if (!buf) { XCloseDisplay(dpy); return 2; }

    /* create client-side XImage (owns data) */
    XImage *xi = XCreateImage(dpy, DefaultVisual(dpy, scr), DefaultDepth(dpy, scr),
                              ZPixmap, 0, (char*)buf, w, h, 32, bytes_per_line);
    if (!xi) { free(buf); XCloseDisplay(dpy); return 2; }

    /* put it to root at (50,50) */
    int put_rc = XPutImage(dpy, root, DefaultGC(dpy, scr), xi, 0, 0, 50, 50, w, h);
    if (put_rc != Success) {
        fprintf(stderr, "XPutImage failed (rc=%d)\n", put_rc);
        XDestroyImage(xi); XCloseDisplay(dpy); return 1;
    }
    XFlush(dpy);

    /* Now request server to XGetImage the same rectangle (50,50,w,h) */
    XImage *got = XGetImage(dpy, root, 50, 50, w, h, AllPlanes, ZPixmap);
    if (!got) {
        fprintf(stderr, "XGetImage failed\n");
        XDestroyImage(xi); XCloseDisplay(dpy); return 1;
    }

    /* compare byte-for-byte using bytes_per_line from returned image */
    size_t got_bytes = (size_t)got->bytes_per_line * (size_t)h;
    size_t orig_bytes = (size_t)bytes_per_line * (size_t)h;

    /* If bytes_per_line differed because server packed differently, compare per pixel */
    if (got_bytes == orig_bytes) {
        if (memcmp(got->data, buf, got_bytes) == 0) {
            fprintf(stderr, "ROUNDTRIP PASSED: got_bytes=%zu\n", got_bytes);
            XDestroyImage(got);
            XDestroyImage(xi); /* frees buf too */
            XCloseDisplay(dpy);
            return 0;
        } else {
            fprintf(stderr, "ROUNDTRIP FAILED: buffers differ (same bpl)\n");
        }
    } else {
        /* fallback: compare per-pixel (assume 32bpp XRGB ordering) */
        int mismatch = 0;
        for (int y = 0; y < h && !mismatch; ++y) {
            unsigned char *rrow = buf + (size_t)y * bytes_per_line;
            unsigned char *grow = (unsigned char*)got->data + (size_t)y * got->bytes_per_line;
            for (int x = 0; x < w; ++x) {
                /* compare RGB triple */
                int off1 = x*4;
                int off2 = x*4;
                if (rrow[off1+0] != grow[off2+0] || rrow[off1+1] != grow[off2+1] || rrow[off1+2] != grow[off2+2]) {
                    mismatch = 1; break;
                }
            }
        }
        if (!mismatch) {
            fprintf(stderr, "ROUNDTRIP PASSED (per-pixel compare)\n");
            XDestroyImage(got); XDestroyImage(xi); XCloseDisplay(dpy); return 0;
        } else {
            fprintf(stderr, "ROUNDTRIP FAILED (per-pixel compare)\n");
        }
    }

    /* Failure case: write diagnostic files for inspection */
    FILE *f = fopen("orig.bin", "wb"); if (f) { fwrite(buf, 1, orig_bytes, f); fclose(f); }
    f = fopen("got.bin", "wb"); if (f) { fwrite(got->data, 1, got_bytes, f); fclose(f); }
    fprintf(stderr, "Wrote orig.bin (%zu bytes) and got.bin (%zu bytes) for inspection\n", orig_bytes, got_bytes);

    XDestroyImage(got);
    XDestroyImage(xi); /* frees buf */
    XCloseDisplay(dpy);
    return 1;
}

