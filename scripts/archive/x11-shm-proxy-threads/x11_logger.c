#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <X11/Xlib.h>

/* Simple LD_PRELOAD logger for a subset of Xlib functions. Each wrapper
   logs the call and forwards to the next library via dlsym(RTLD_NEXT,...).
   Build: gcc -shared -fPIC -o libx11log.so x11_logger.c -ldl
*/

#define LOG(fmt, ...) fprintf(stderr, "[x11log] " fmt, ##__VA_ARGS__)

static Display *(*real_XOpenDisplay)(const char *) = NULL;
Display *XOpenDisplay(const char *dpy) {
    if (!real_XOpenDisplay) real_XOpenDisplay = dlsym(RTLD_NEXT, "XOpenDisplay");
    LOG("XOpenDisplay('%s')\n", dpy ? dpy : "(null)");
    return real_XOpenDisplay ? real_XOpenDisplay(dpy) : NULL;
}

static int (*real_XCloseDisplay)(Display *) = NULL;
int XCloseDisplay(Display *dpy) {
    if (!real_XCloseDisplay) real_XCloseDisplay = dlsym(RTLD_NEXT, "XCloseDisplay");
    LOG("XCloseDisplay(%p)\n", (void*)dpy);
    return real_XCloseDisplay ? real_XCloseDisplay(dpy) : 0;
}

static void (*real_XLockDisplay)(Display *) = NULL;
void XLockDisplay(Display *dpy) {
    if (!real_XLockDisplay) real_XLockDisplay = dlsym(RTLD_NEXT, "XLockDisplay");
    LOG("XLockDisplay(%p)\n", (void*)dpy);
    if (real_XLockDisplay) real_XLockDisplay(dpy);
}

static void (*real_XUnlockDisplay)(Display *) = NULL;
void XUnlockDisplay(Display *dpy) {
    if (!real_XUnlockDisplay) real_XUnlockDisplay = dlsym(RTLD_NEXT, "XUnlockDisplay");
    LOG("XUnlockDisplay(%p)\n", (void*)dpy);
    if (real_XUnlockDisplay) real_XUnlockDisplay(dpy);
}

static void (*real_XFlush)(Display *) = NULL;
int XFlush(Display *dpy) {
    if (!real_XFlush) real_XFlush = dlsym(RTLD_NEXT, "XFlush");
    LOG("XFlush(%p)\n", (void*)dpy);
    if (real_XFlush) { real_XFlush(dpy); return 1; }
    return 0;
}

static int (*real_XSync)(Display *, Bool) = NULL;
int XSync(Display *dpy, Bool discard) {
    if (!real_XSync) real_XSync = dlsym(RTLD_NEXT, "XSync");
    LOG("XSync(%p, discard=%d)\n", (void*)dpy, (int)discard);
    return real_XSync ? real_XSync(dpy, discard) : 0;
}

static Window (*real_XCreateWindow)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual*, unsigned long, XSetWindowAttributes*) = NULL;
Window XCreateWindow(Display *dpy, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int klass, Visual *visual, unsigned long valuemask, XSetWindowAttributes *attributes) {
    if (!real_XCreateWindow) real_XCreateWindow = dlsym(RTLD_NEXT, "XCreateWindow");
    LOG("XCreateWindow(dpy=%p, parent=0x%lx)\n", (void*)dpy, (unsigned long)parent);
    return real_XCreateWindow ? real_XCreateWindow(dpy, parent, x, y, width, height, border_width, depth, klass, visual, valuemask, attributes) : 0;
}

static int (*real_XMapWindow)(Display*, Window) = NULL;
int XMapWindow(Display *dpy, Window w) {
    if (!real_XMapWindow) real_XMapWindow = dlsym(RTLD_NEXT, "XMapWindow");
    LOG("XMapWindow(dpy=%p, win=0x%lx)\n", (void*)dpy, (unsigned long)w);
    return real_XMapWindow ? real_XMapWindow(dpy, w) : 0;
}

static int (*real_XDestroyWindow)(Display*, Window) = NULL;
int XDestroyWindow(Display *dpy, Window w) {
    if (!real_XDestroyWindow) real_XDestroyWindow = dlsym(RTLD_NEXT, "XDestroyWindow");
    LOG("XDestroyWindow(dpy=%p, win=0x%lx)\n", (void*)dpy, (unsigned long)w);
    return real_XDestroyWindow ? real_XDestroyWindow(dpy, w) : 0;
}

static Atom (*real_XInternAtom)(Display*, const char*, Bool) = NULL;
Atom XInternAtom(Display *dpy, const char *name, Bool only_if_exists) {
    if (!real_XInternAtom) real_XInternAtom = dlsym(RTLD_NEXT, "XInternAtom");
    LOG("XInternAtom(dpy=%p, name=%s)\n", (void*)dpy, name ? name : "(null)");
    return real_XInternAtom ? real_XInternAtom(dpy, name, only_if_exists) : None;
}

static unsigned long (*real_XWhitePixel)(Display*, int) = NULL;
unsigned long XWhitePixel(Display *dpy, int screen) {
    if (!real_XWhitePixel) real_XWhitePixel = dlsym(RTLD_NEXT, "XWhitePixel");
    LOG("XWhitePixel(dpy=%p, screen=%d)\n", (void*)dpy, screen);
    return real_XWhitePixel ? real_XWhitePixel(dpy, screen) : 0;
}

static Status (*real_XGetWindowAttributes)(Display*, Window, XWindowAttributes*) = NULL;
Status XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *out) {
    if (!real_XGetWindowAttributes) real_XGetWindowAttributes = dlsym(RTLD_NEXT, "XGetWindowAttributes");
    LOG("XGetWindowAttributes(dpy=%p, win=0x%lx)\n", (void*)dpy, (unsigned long)w);
    return real_XGetWindowAttributes ? real_XGetWindowAttributes(dpy, w, out) : 0;
}

/* Optionally add more wrappers if needed */

