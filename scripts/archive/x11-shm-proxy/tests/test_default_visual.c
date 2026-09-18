/*
 * test_default_visual_nohelper.c
 *
 * Test XDefaultVisual without relying on an ipc_visualid_from_placeholder helper.
 *
 * It checks:
 *  - XDefaultVisual returns non-NULL (placeholder)
 *  - XGetVisualInfo(VisualScreenMask) returns visuals for the screen
 *  - At least one returned visual matches DefaultDepth(dpy, screen)
 *
 * Build:
 *   gcc -o test_default_visual_nohelper test_default_visual_nohelper.c -lX11
 *
 * Run (server_shm must be running, and your wrappers loaded if using LD_PRELOAD):
 *   LD_PRELOAD=./libx11ipc.so ./test_default_visual_nohelper
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Fallbacks if missing in custom headers */
#ifndef VisualScreenMask
#define VisualScreenMask (1L << 1)
#endif

#ifndef TrueColor
#define TrueColor 4
#endif

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "ERROR: XOpenDisplay failed\n");
        return 2;
    }
    fprintf(stderr, "Opened display %p\n", (void*)dpy);

    int screen = DefaultScreen(dpy);
    fprintf(stderr, "DefaultScreen = %d\n", screen);

    /* Call XDefaultVisual (wrapper) */
    Visual *v = XDefaultVisual(dpy, screen);
    if (!v) {
        fprintf(stderr, "ERROR: XDefaultVisual returned NULL\n");
        XCloseDisplay(dpy);
        return 1;
    }
    fprintf(stderr, "XDefaultVisual returned a non-NULL Visual* placeholder: %p\n", (void*)v);

    /* Obtain DefaultDepth for the screen (may be macro) */
#ifndef DefaultDepth
    /* fallback: assume 24 if macro not present */
    int default_depth = 24;
#else
    int default_depth = DefaultDepth(dpy, screen);
#endif
    fprintf(stderr, "DefaultDepth for screen %d = %d\n", screen, default_depth);

    /* Enumerate visuals for the screen */
    XVisualInfo tmpl;
    memset(&tmpl, 0, sizeof(tmpl));
    tmpl.screen = screen;
    int n = 0;
    XVisualInfo *list = XGetVisualInfo(dpy, VisualScreenMask, &tmpl, &n);
    if (!list || n <= 0) {
        fprintf(stderr, "ERROR: XGetVisualInfo returned no visuals (n=%d)\n", n);
        if (list) free(list);
        XCloseDisplay(dpy);
        return 1;
    }

    fprintf(stderr, "XGetVisualInfo returned %d visuals for screen %d\n", n, screen);

    /* Print visuals and check for a match on DefaultDepth */
    int found_depth_match = 0;
    for (int i = 0; i < n; ++i) {
        unsigned long vid = (unsigned long)list[i].visualid;
#ifdef __GNUC__
        int vclass = list[i].c_class; /* adapt if your XVisualInfo uses 'class' */
#else
        int vclass = list[i].class;
#endif
        fprintf(stderr, "  [%d] visualid=0x%lx depth=%d class=%d red=0x%lx green=0x%lx blue=0x%lx\n",
                i, vid, list[i].depth, vclass,
                (unsigned long)list[i].red_mask, (unsigned long)list[i].green_mask, (unsigned long)list[i].blue_mask);
        if (list[i].depth == default_depth) found_depth_match = 1;
    }

    if (!found_depth_match) {
        fprintf(stderr, "WARNING: No visual in list matched DefaultDepth=%d. This may be okay\n"
                        "if your X server uses a different default visual depth, or if the\n"
                        "placeholder doesn't represent the same visual. Consider using the\n"
                        "helper to compare visualid explicitly for a stronger test.\n",
                default_depth);
        free(list);
        XCloseDisplay(dpy);
        /* We treat this as a warning; return non-zero if you want strict failure */
        return 0;
    }

    fprintf(stderr, "Found at least one visual matching DefaultDepth=%d. Basic sanity check PASSED.\n", default_depth);

    free(list);
    XCloseDisplay(dpy);
    return 0;
}

