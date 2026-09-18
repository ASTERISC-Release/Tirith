/* Header to replace some X11 macros that access Display internals with
 * wrapper functions so they can be intercepted/logged.
 * Include this early in client sources to remap macros to functions.
 */
#ifndef INTERCEPT_X11_MACROS_H
#define INTERCEPT_X11_MACROS_H

#include <X11/Xlib.h>

/* Replace a few commonly-used macros with wrapper functions. These wrappers
 * are implemented in the interceptor library and can log accesses. Add more
 * mappings here as needed.
 */
#ifdef DefaultScreen
#undef DefaultScreen
#endif
#define DefaultScreen(d) intercepted_DefaultScreen(d)

#ifdef DefaultRootWindow
#undef DefaultRootWindow
#endif
#define DefaultRootWindow(d) intercepted_DefaultRootWindow(d)

#ifdef RootWindow
#undef RootWindow
#endif
#define RootWindow(d,s) intercepted_RootWindow(d,s)

/* Prototypes - implemented in interceptor.c */
int intercepted_DefaultScreen(Display *d);
Window intercepted_DefaultRootWindow(Display *d);
Window intercepted_RootWindow(Display *d, int screen);

#endif /* INTERCEPT_X11_MACROS_H */
