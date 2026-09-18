/*
 * test_match_visual.c
 *
 * Test program for XMatchVisualInfo wrapper (IPC).
 * - tries to include project's common.h (preferred),
 * - otherwise declares a fallback prototype.
 *
 * Build:
 *   gcc -o test_match_visual test_match_visual.c -lX11
 *
 * Run (server must be running and wrappers loaded if needed):
 *   LD_PRELOAD=./libx11ipc.so ./test_match_visual
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Prefer your project's common.h if it exists (it should declare the wrapper) */
#if defined(__has_include)
#  if __has_include("common.h")
#    include "common.h"
#    define HAVE_COMMON_H 1
#  endif
#endif


int main(void)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "XOpenDisplay failed\n");
        return 1;
    }
    fprintf(stderr, "Opened display: %p\n", (void*)dpy);

    int screen = DefaultScreen(dpy);
    int depth = 24;
    int vclass = TrueColor;

    XVisualInfo vinfo;
    /* Zero it so any unused fields are deterministic */
    memset(&vinfo, 0, sizeof(vinfo));

    int ok = XMatchVisualInfo(dpy, screen, depth, vclass, &vinfo);

    if (!ok) {
        fprintf(stderr, "XMatchVisualInfo: no match for depth=%d class=%d on screen=%d\n",
                depth, vclass, screen);
    } else {
        /* print canonical fields; adapt if your XVisualInfo uses 'class' instead of 'c_class' */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif
        fprintf(stderr, "XMatchVisualInfo succeeded:\n");
        fprintf(stderr, "  visualid=0x%lx\n", (unsigned long)vinfo.visualid);
        fprintf(stderr, "  depth=%d\n", vinfo.depth);
        /* attempt to print c_class; if your struct uses 'class' rename below */
        #if defined(__has_include) && __has_include("common.h")
        /* assume common.h-defined XVisualInfo uses c_class */
        fprintf(stderr, "  class=%d\n", vinfo.c_class);
        #else
        /* best-effort: try c_class, otherwise print placeholder */
        fprintf(stderr, "  class (c_class)=%d\n", vinfo.c_class);
        #endif

        fprintf(stderr, "  red_mask=0x%lx  green_mask=0x%lx  blue_mask=0x%lx\n",
                (unsigned long)vinfo.red_mask,
                (unsigned long)vinfo.green_mask,
                (unsigned long)vinfo.blue_mask);
        fprintf(stderr, "  colormap_size=%d  bits_per_rgb=%d\n",
                vinfo.colormap_size, vinfo.bits_per_rgb);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    }

    XCloseDisplay(dpy);
    return 0;
}

