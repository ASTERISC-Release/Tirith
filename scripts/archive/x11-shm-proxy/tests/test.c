/* test_x.c - simple test that opens a display, creates a window, maps it and runs a short loop */
#include <stdio.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "test_x: XOpenDisplay failed\n"); return 1; }
    int scr = DefaultScreen(dpy);

    fprintf(stderr, "test_x: display opened (screen = %d), creating window\n", scr);
    Window w = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 10, 10, 300, 120, 2,
                                   BlackPixel(dpy, scr), WhitePixel(dpy, scr));
    if (!w) { 
        fprintf(stderr, "test_x: XCreateSimpleWindow failed\n"); 
        XCloseDisplay(dpy); 
        return 1; 
    }

    XSelectInput(dpy, w, ExposureMask | ButtonPressMask);
    XMapWindow(dpy, w);
    XFlush(dpy);
    fprintf(stderr, "test_x: window mapped, running loop for 10s\n");
    time_t start = time(NULL);
    XEvent ev;
    while (time(NULL) - start < 10) {
        XNextEvent(dpy, &ev);
        if (ev.type == Expose) {
            fprintf(stderr, "test_x: got Expose\n");
        } else if (ev.type == ButtonPress) {
            fprintf(stderr, "test_x: ButtonPress\n");
            break;
        }
    }
    XDestroyWindow(dpy, w);
    XCloseDisplay(dpy);
    return 0;
}

