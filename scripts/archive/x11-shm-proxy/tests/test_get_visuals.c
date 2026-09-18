/*
 * test_get_visuals.c
 *
 * Test program to validate IPC-backed XGetVisualInfo and XMatchVisualInfo.
 *
 * Build:
 *   gcc -o test_get_visuals test_get_visuals.c -lX11
 *
 * Run:
 *   ./server_shm &                # start server in another shell
 *   LD_PRELOAD=./libx11ipc.so ./test_get_visuals
 *
 * The program prints all visuals for the DefaultScreen, checks each with
 * XMatchVisualInfo, then attempts a targeted lookup for 24-bit TrueColor.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Fallback visual masks if not defined in your headers */
#ifndef VisualScreenMask
#define VisualScreenMask         (1L << 1)
#endif
#ifndef VisualDepthMask
#define VisualDepthMask          (1L << 2)
#endif
#ifndef VisualClassMask
#define VisualClassMask          (1L << 3)
#endif
#ifndef VisualRedMask
#define VisualRedMask            (1L << 4)
#endif
#ifndef VisualGreenMask
#define VisualGreenMask          (1L << 5)
#endif
#ifndef VisualBlueMask
#define VisualBlueMask           (1L << 6)
#endif

/* fallback visual classes */
#ifndef TrueColor
#define TrueColor 4
#endif

/* Prefer project's prototype if present via common.h */
#if defined(__has_include)
# if __has_include("common.h")
#  include "common.h"
# endif
#endif

/* Fallback prototype for wrapper if common.h didn't provide it */
#ifndef XGetVisualInfo
/* We still declare the standard signature to avoid implicit decl warnings.
   The client wrapper should provide this. */
extern XVisualInfo *XGetVisualInfo(Display *display, long vinfo_mask,
                                   XVisualInfo *vinfo_template, int *nitems_return);
#endif

#ifndef XMatchVisualInfo
extern int XMatchVisualInfo(Display *display, int screen, int depth, int vclass, XVisualInfo *vinfo);
#endif

int print_visual(const XVisualInfo *v, int idx) {
    if (!v) return -1;
    printf(" [%02d] visualid=0x%lx depth=%d class=%d red_mask=0x%lx green_mask=0x%lx blue_mask=0x%lx colormap_size=%d bits_per_rgb=%d\n",
           idx,
           (unsigned long)v->visualid,
           v->depth,
#ifdef __GNUC__
           v->c_class,    /* if your XVisualInfo uses c_class */
#else
           v->class,      /* otherwise */
#endif
           (unsigned long)v->red_mask,
           (unsigned long)v->green_mask,
           (unsigned long)v->blue_mask,
           v->colormap_size,
           v->bits_per_rgb);
    return 0;
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "ERROR: XOpenDisplay failed\n");
        return 2;
    }
    printf("Opened display %p\n", (void*)dpy);

    int screen = DefaultScreen(dpy);
    printf("DefaultScreen = %d\n", screen);

    /* 1) Enumerate all visuals for the screen */
    XVisualInfo tmpl;
    memset(&tmpl, 0, sizeof(tmpl));
    tmpl.screen = screen;
    long mask = VisualScreenMask;

    int n = 0;
    XVisualInfo *list = XGetVisualInfo(dpy, mask, &tmpl, &n);
    if (!list || n <= 0) {
        printf("XGetVisualInfo: returned no visuals (n=%d)\n", n);
    } else {
        printf("XGetVisualInfo: returned %d visuals\n", n);
        for (int i = 0; i < n; ++i) {
            print_visual(&list[i], i);
            /* For each returned visual, verify XMatchVisualInfo returns a match */
            XVisualInfo verify;
            memset(&verify, 0, sizeof(verify));
            int matched = XMatchVisualInfo(dpy, screen, list[i].depth,
#ifdef __GNUC__
                                          list[i].c_class,
#else
                                          list[i].class,
#endif
                                          &verify);
            if (!matched) {
                fprintf(stderr, "  WARNING: XMatchVisualInfo did NOT match visualid=0x%lx (depth=%d class=%d)\n",
                        (unsigned long)list[i].visualid, list[i].depth,
#ifdef __GNUC__
                        list[i].c_class);
#else
                        list[i].class);
#endif
            } else {
                printf("  Verified by XMatchVisualInfo: visualid=0x%lx depth=%d\n",
                       (unsigned long)verify.visualid, verify.depth);
            }
        }
    }

    if (list) free(list);

    /* 2) Try a targeted lookup for 24-bit TrueColor */
    memset(&tmpl, 0, sizeof(tmpl));
    tmpl.screen = screen;
    tmpl.depth = 24;
#ifdef __GNUC__
    tmpl.c_class = TrueColor;
#else
    tmpl.class = TrueColor;
#endif
    mask = VisualScreenMask | VisualDepthMask | VisualClassMask;
    int m = 0;
    XVisualInfo *best = XGetVisualInfo(dpy, mask, &tmpl, &m);
    if (!best || m <= 0) {
        printf("Targeted XGetVisualInfo(depth=24 class=TrueColor) returned no results (m=%d)\n", m);
    } else {
        printf("Targeted XGetVisualInfo: returned %d visuals\n", m);
        for (int i = 0; i < m; ++i) {
            print_visual(&best[i], i);
        }
    }
    if (best) free(best);

    XCloseDisplay(dpy);
    printf("Done.\n");
    return 0;
}

