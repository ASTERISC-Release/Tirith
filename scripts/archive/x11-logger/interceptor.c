#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <stdarg.h>

// Function pointers for real X11 functions
static Display* (*real_XOpenDisplay)(const char*) = NULL;
static int (*real_XCloseDisplay)(Display*) = NULL;
static Window (*real_XCreateSimpleWindow)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long) = NULL;
static int (*real_XMapWindow)(Display*, Window) = NULL;
static int (*real_XNextEvent)(Display*, XEvent*) = NULL;
static int (*real_XPending)(Display*) = NULL;
static int (*real_XFlush)(Display*) = NULL;
static int (*real_XSync)(Display*, Bool) = NULL;
static int (*real_XMoveWindow)(Display*, Window, int, int) = NULL;
static int (*real_XResizeWindow)(Display*, Window, unsigned int, unsigned int) = NULL;
static int (*real_XDestroyWindow)(Display*, Window) = NULL;
static int (*real_XRaiseWindow)(Display*, Window) = NULL;
static int (*real_XLowerWindow)(Display*, Window) = NULL;
static Atom (*real_XInternAtom)(Display*, const char*, Bool) = NULL;
static int (*real_XStoreName)(Display*, Window, const char*) = NULL;
static Window (*real_XCreateWindow)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual*, unsigned long, XSetWindowAttributes*) = NULL;
static int (*real_XConfigureWindow)(Display*, Window, unsigned int, XWindowChanges*) = NULL;
static int (*real_XChangeProperty)(Display*, Window, Atom, Atom, int, int, const unsigned char*, int) = NULL;
static Status (*real_XGetWindowAttributes)(Display*, Window, XWindowAttributes*) = NULL;
static int (*real_XSelectInput)(Display*, Window, long) = NULL;
static int (*real_XSendEvent)(Display*, Window, Bool, long, XEvent*) = NULL;
static Pixmap (*real_XCreatePixmap)(Display*, Drawable, unsigned int, unsigned int, unsigned int) = NULL;
static int (*real_XFreePixmap)(Display*, Pixmap) = NULL;
static char* (*real_XGetAtomName)(Display*, Atom) = NULL;

// Logging function
static void log_call(const char* function_name, const char* format, ...) {
    if (getenv("X11_INTERCEPT_LOG")) {
        va_list args;
        va_start(args, format);
        fprintf(stderr, "[X11-INTERCEPT] %s(", function_name);
        vfprintf(stderr, format, args);
        fprintf(stderr, ")\n");
        va_end(args);
    }
}

// Initialize real function pointers
static void init_real_functions() {
    static int initialized = 0;
    if (initialized) return;
    
    real_XOpenDisplay = dlsym(RTLD_NEXT, "XOpenDisplay");
    real_XCloseDisplay = dlsym(RTLD_NEXT, "XCloseDisplay");
    real_XCreateSimpleWindow = dlsym(RTLD_NEXT, "XCreateSimpleWindow");
    real_XMapWindow = dlsym(RTLD_NEXT, "XMapWindow");
    real_XNextEvent = dlsym(RTLD_NEXT, "XNextEvent");
    real_XPending = dlsym(RTLD_NEXT, "XPending");
    real_XFlush = dlsym(RTLD_NEXT, "XFlush");
    real_XSync = dlsym(RTLD_NEXT, "XSync");
    real_XMoveWindow = dlsym(RTLD_NEXT, "XMoveWindow");
    real_XResizeWindow = dlsym(RTLD_NEXT, "XResizeWindow");
    real_XDestroyWindow = dlsym(RTLD_NEXT, "XDestroyWindow");
    real_XRaiseWindow = dlsym(RTLD_NEXT, "XRaiseWindow");
    real_XLowerWindow = dlsym(RTLD_NEXT, "XLowerWindow");
    real_XInternAtom = dlsym(RTLD_NEXT, "XInternAtom");
    real_XStoreName = dlsym(RTLD_NEXT, "XStoreName");
    real_XCreateWindow = dlsym(RTLD_NEXT, "XCreateWindow");
    real_XConfigureWindow = dlsym(RTLD_NEXT, "XConfigureWindow");
    real_XChangeProperty = dlsym(RTLD_NEXT, "XChangeProperty");
    real_XGetWindowAttributes = dlsym(RTLD_NEXT, "XGetWindowAttributes");
    real_XSelectInput = dlsym(RTLD_NEXT, "XSelectInput");
    real_XSendEvent = dlsym(RTLD_NEXT, "XSendEvent");
    real_XCreatePixmap = dlsym(RTLD_NEXT, "XCreatePixmap");
    real_XFreePixmap = dlsym(RTLD_NEXT, "XFreePixmap");
    real_XGetAtomName = dlsym(RTLD_NEXT, "XGetAtomName");
    
    initialized = 1;
            /* No direct dlsym for DefaultRootWindow/DefaultScreen/RootWindow because
             * they are macros in headers. We'll implement wrappers that use public
             * Xlib functions to get the same values and log them.
             */
}

// Intercepted X11 functions
Display* XOpenDisplay(const char* display_name) {
    init_real_functions();
    log_call("XOpenDisplay", "%s", display_name ? display_name : "NULL");
    
    Display* result = real_XOpenDisplay(display_name);
    log_call("XOpenDisplay", "returned %p", result);
    return result;
}

int XCloseDisplay(Display* display) {
    init_real_functions();
    log_call("XCloseDisplay", "%p", display);
    
    int result = real_XCloseDisplay(display);
    log_call("XCloseDisplay", "returned %d", result);
    return result;
}

Window XCreateSimpleWindow(Display* display, Window parent, int x, int y, 
                          unsigned int width, unsigned int height, 
                          unsigned int border_width, unsigned long border, 
                          unsigned long background) {
    init_real_functions();
    log_call("XCreateSimpleWindow", "display=%p, parent=%lu, x=%d, y=%d, w=%u, h=%u", 
             display, parent, x, y, width, height);
    
    Window result = real_XCreateSimpleWindow(display, parent, x, y, width, height, 
                                           border_width, border, background);
    log_call("XCreateSimpleWindow", "returned %lu", result);
    return result;
}

Window XCreateWindow(Display* display, Window parent, int x, int y,
                     unsigned int width, unsigned int height,
                     unsigned int border_width, int depth, unsigned int _class,
                     Visual* visual, unsigned long valuemask, XSetWindowAttributes* attributes) {
    init_real_functions();
    log_call("XCreateWindow", "display=%p, parent=%lu, x=%d, y=%d, w=%u, h=%u, depth=%d, class=%u, vm=%lu",
             display, parent, x, y, width, height, depth, _class, valuemask);

    Window result = real_XCreateWindow(display, parent, x, y, width, height, border_width, depth, _class, visual, valuemask, attributes);
    log_call("XCreateWindow", "returned %lu", result);
    return result;
}

int XConfigureWindow(Display* display, Window w, unsigned int value_mask, XWindowChanges* changes) {
    init_real_functions();
    log_call("XConfigureWindow", "display=%p, window=%lu, mask=%u", display, w, value_mask);

    int result = real_XConfigureWindow(display, w, value_mask, changes);
    log_call("XConfigureWindow", "returned %d", result);
    return result;
}

int XChangeProperty(Display* display, Window w, Atom property, Atom type, int format, int mode, const unsigned char* data, int nelements) {
    init_real_functions();
    log_call("XChangeProperty", "display=%p, window=%lu, property=%lu, type=%lu, format=%d, mode=%d, nelements=%d",
             display, w, (unsigned long)property, (unsigned long)type, format, mode, nelements);

    int result = real_XChangeProperty(display, w, property, type, format, mode, data, nelements);
    log_call("XChangeProperty", "returned %d", result);
    return result;
}

Status XGetWindowAttributes(Display* display, Window w, XWindowAttributes* window_attributes_return) {
    init_real_functions();
    log_call("XGetWindowAttributes", "display=%p, window=%lu", display, w);

    Status result = real_XGetWindowAttributes(display, w, window_attributes_return);
    log_call("XGetWindowAttributes", "returned %d, x=%d y=%d w=%u h=%u",
             result, window_attributes_return ? window_attributes_return->x : -1,
             window_attributes_return ? window_attributes_return->y : -1,
             window_attributes_return ? window_attributes_return->width : 0,
             window_attributes_return ? window_attributes_return->height : 0);
    return result;
}

int XSelectInput(Display* display, Window w, long event_mask) {
    init_real_functions();
    log_call("XSelectInput", "display=%p, window=%lu, mask=%ld", display, w, event_mask);

    int result = real_XSelectInput(display, w, event_mask);
    log_call("XSelectInput", "returned %d", result);
    return result;
}

int XSendEvent(Display* display, Window w, Bool propagate, long event_mask, XEvent* event) {
    init_real_functions();
    log_call("XSendEvent", "display=%p, window=%lu, propagate=%d, mask=%ld, event=%p", display, w, propagate, event_mask, event);

    int result = real_XSendEvent(display, w, propagate, event_mask, event);
    log_call("XSendEvent", "returned %d", result);
    return result;
}

Pixmap XCreatePixmap(Display* display, Drawable d, unsigned int width, unsigned int height, unsigned int depth) {
    init_real_functions();
    log_call("XCreatePixmap", "display=%p, drawable=%lu, w=%u, h=%u, depth=%u", display, (unsigned long)d, width, height, depth);

    Pixmap result = real_XCreatePixmap(display, d, width, height, depth);
    log_call("XCreatePixmap", "returned %lu", (unsigned long)result);
    return result;
}

int XFreePixmap(Display* display, Pixmap pixmap) {
    init_real_functions();
    log_call("XFreePixmap", "display=%p, pixmap=%lu", display, (unsigned long)pixmap);

    int result = real_XFreePixmap(display, pixmap);
    log_call("XFreePixmap", "returned %d", result);
    return result;
}

char* XGetAtomName(Display* display, Atom atom) {
    init_real_functions();
    log_call("XGetAtomName", "display=%p, atom=%lu", display, (unsigned long)atom);

    char* result = real_XGetAtomName(display, atom);
    log_call("XGetAtomName", "returned %s", result ? result : "NULL");
    return result;
}

int XMapWindow(Display* display, Window w) {
    init_real_functions();
    log_call("XMapWindow", "display=%p, window=%lu", display, w);
    
    int result = real_XMapWindow(display, w);
    log_call("XMapWindow", "returned %d", result);
    return result;
}

int XMoveWindow(Display* display, Window w, int x, int y) {
    init_real_functions();
    log_call("XMoveWindow", "display=%p, window=%lu, x=%d, y=%d", display, w, x, y);

    int result = real_XMoveWindow(display, w, x, y);
    log_call("XMoveWindow", "returned %d", result);
    return result;
}

int XResizeWindow(Display* display, Window w, unsigned int width, unsigned int height) {
    init_real_functions();
    log_call("XResizeWindow", "display=%p, window=%lu, w=%u, h=%u", display, w, width, height);

    int result = real_XResizeWindow(display, w, width, height);
    log_call("XResizeWindow", "returned %d", result);
    return result;
}

int XDestroyWindow(Display* display, Window w) {
    init_real_functions();
    log_call("XDestroyWindow", "display=%p, window=%lu", display, w);

    int result = real_XDestroyWindow(display, w);
    log_call("XDestroyWindow", "returned %d", result);
    return result;
}

int XRaiseWindow(Display* display, Window w) {
    init_real_functions();
    log_call("XRaiseWindow", "display=%p, window=%lu", display, w);

    int result = real_XRaiseWindow(display, w);
    log_call("XRaiseWindow", "returned %d", result);
    return result;
}

int XLowerWindow(Display* display, Window w) {
    init_real_functions();
    log_call("XLowerWindow", "display=%p, window=%lu", display, w);

    int result = real_XLowerWindow(display, w);
    log_call("XLowerWindow", "returned %d", result);
    return result;
}

Atom XInternAtom(Display* display, const char* atom_name, Bool only_if_exists) {
    init_real_functions();
    log_call("XInternAtom", "display=%p, name=%s, only_if_exists=%d", display, atom_name ? atom_name : "NULL", only_if_exists);

    Atom result = real_XInternAtom(display, atom_name, only_if_exists);
    log_call("XInternAtom", "returned %lu", (unsigned long)result);
    return result;
}

int XStoreName(Display* display, Window w, const char* window_name) {
    init_real_functions();
    log_call("XStoreName", "display=%p, window=%lu, name=%s", display, w, window_name ? window_name : "NULL");

    int result = real_XStoreName(display, w, window_name);
    log_call("XStoreName", "returned %d", result);
    return result;
}

    /* Implement wrappers for commonly used macros so client code that includes
     * intercept_x11_macros.h will call these instead of the macro. These use
     * public Xlib functions where available.
     */
    int intercepted_DefaultScreen(Display *d) {
        init_real_functions();
        int screen = DefaultScreen(d);
        /* Better: use the public function DefaultScreen() is a macro; use
         * ScreenNumberOfDisplay if present; to be safe, call DefaultScreen(d)
         * behavior: the macro expands to (d->default_screen). We can't access
         * internals portably, so call XDefaultScreen which is a function in some
         * implementations — but not standard. Instead use ScreenNumberOfDisplay
         * if available via macros; simplest portable approach: use XDefaultScreen
         * if available via Xlib.h. Fallback to 0.
         */
        log_call("intercepted_DefaultScreen", "display=%p returned %d", d, screen);
        return screen;
    }

    Window intercepted_DefaultRootWindow(Display *d) {
        init_real_functions();
        Window w = DefaultRootWindow(d); /* macro expands to RootWindow(d, DefaultScreen(d)) */
        log_call("intercepted_DefaultRootWindow", "display=%p returned %lu", d, (unsigned long)w);
        return w;
    }

    Window intercepted_RootWindow(Display *d, int screen) {
        init_real_functions();
        Window w = RootWindow(d, screen);
        log_call("intercepted_RootWindow", "display=%p screen=%d returned %lu", d, screen, (unsigned long)w);
        return w;
    }

int XNextEvent(Display* display, XEvent* event) {
    init_real_functions();
    log_call("XNextEvent", "display=%p, event=%p", display, event);
    
    int result = real_XNextEvent(display, event);
    log_call("XNextEvent", "returned %d, event_type=%d", result, event ? event->type : -1);
    return result;
}

int XPending(Display* display) {
    init_real_functions();
    log_call("XPending", "display=%p", display);
    
    int result = real_XPending(display);
    log_call("XPending", "returned %d", result);
    return result;
}

int XFlush(Display* display) {
    init_real_functions();
    log_call("XFlush", "display=%p", display);
    
    int result = real_XFlush(display);
    log_call("XFlush", "returned %d", result);
    return result;
}

int XSync(Display* display, Bool discard) {
    init_real_functions();
    log_call("XSync", "display=%p, discard=%d", display, discard);
    
    int result = real_XSync(display, discard);
    log_call("XSync", "returned %d", result);
    return result;
}