#ifndef X11_DEFINITIONS_H
#define X11_DEFINITIONS_H

/* Function declarations */
Display *XOpenDisplay(const char *display_name);
int XCloseDisplay(Display *display);
Window XCreateSimpleWindow(Display *display, Window parent, int x, int y,
                          unsigned int width, unsigned int height,
                          unsigned int border_width, unsigned long border,
                          unsigned long background);
Window XCreateWindow(Display *display, Window parent, int x, int y,
                     unsigned int width, unsigned int height, unsigned int border_width,
                     int depth, unsigned int class, void *visual,
                     unsigned long valuemask, XSetWindowAttributes *attributes);
int XMapWindow(Display *display, Window w);
int XUnmapWindow(Display *display, Window w);
int XConfigureWindow(Display *display, Window w, unsigned int value_mask, XWindowChanges *changes);
int XDestroyWindow(Display *display, Window w);
int XFlush(Display *display);
int XPending(Display *display);
int XNextEvent(Display *display, XEvent *event_return);
int XSelectInput(Display *display, Window w, long event_mask);
int XDefaultScreen(Display *display);
Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists);
int XChangeProperty(Display *display, Window w, Atom property, Atom type,
                   int format, int mode, const unsigned char *data, int nelements);
Status XSetWMProtocols(Display *display, Window w, Atom *protocols, int count);
int XStoreName(Display *display, Window w, const char *window_name);
Status XSetWMHints(Display *display, Window w, XWMHints *wmhints);
XWMHints *XAllocWMHints(void);
int XFree(void *data);
int XSetStandardProperties(Display *display, Window w, const char *window_name,
                          const char *icon_name, Pixmap icon_pixmap,
                          char **argv, int argc, XSizeHints *hints);
int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer,
                  KeySym *keysym_return, XComposeStatus *status_in_out);
int XEventsQueued(Display *display, int mode);
int XConnectionNumber(Display *display);
Status XGetGeometry(Display *display, Drawable d, Window *root_return,
                   int *x_return, int *y_return, unsigned int *width_return,
                   unsigned int *height_return, unsigned int *border_width_return,
                   unsigned int *depth_return);
int XMoveWindow(Display *display, Window w, int x, int y);
int XResizeWindow(Display *display, Window w, unsigned int width, unsigned int height);

#endif /* X11_DEFINITIONS_H */