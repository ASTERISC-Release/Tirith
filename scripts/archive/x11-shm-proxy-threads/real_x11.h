#ifndef REAL_X11_H
#define REAL_X11_H

#define _GNU_SOURCE
#include <X11/Xlib.h>

/* existing typedefs... */
typedef Display *(*XOpenDisplay_fn_t)(const char *);
typedef int (*XCloseDisplay_fn_t)(Display *);
typedef Window (*XCreateWindow_fn_t)(Display *, Window, int, int, unsigned int, unsigned int,
                                     unsigned int, int, unsigned int, Visual *, unsigned long,
                                     XSetWindowAttributes *);
typedef int (*XMapWindow_fn_t)(Display *, Window);
typedef int (*XDestroyWindow_fn_t)(Display *, Window);
typedef Atom (*XInternAtom_fn_t)(Display *, const char *, Bool);
typedef void (*XFlush_fn_t)(Display *);
typedef int (*XSync_fn_t)(Display *, Bool);
typedef Status (*XGetWindowAttributes_fn_t)(Display *, Window, XWindowAttributes *);
typedef void (*XLockDisplay_fn_t)(Display *);
typedef void (*XUnlockDisplay_fn_t)(Display *);
typedef int (*XDefaultScreen_fn_t)(Display *);

/* existing accessors... */
XOpenDisplay_fn_t get_real_XOpenDisplay(void);
XCloseDisplay_fn_t get_real_XCloseDisplay(void);
XCreateWindow_fn_t get_real_XCreateWindow(void);
XMapWindow_fn_t get_real_XMapWindow(void);
XDestroyWindow_fn_t get_real_XDestroyWindow(void);
XInternAtom_fn_t get_real_XInternAtom(void);
XFlush_fn_t get_real_XFlush(void);
XSync_fn_t get_real_XSync(void);
XGetWindowAttributes_fn_t get_real_XGetWindowAttributes(void);
XLockDisplay_fn_t get_real_XLockDisplay(void);
XUnlockDisplay_fn_t get_real_XUnlockDisplay(void);
XDefaultScreen_fn_t get_real_XDefaultScreen(void);

#endif /* REAL_X11_H */
