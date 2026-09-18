#include "intercept_x11_macros.h"
#include <X11/Xatom.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>

int main() {
    Display *display;
    Window window;
    XEvent event;

    // Open connection to X server
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    // Create a simple window
    window = XCreateSimpleWindow(display, DefaultRootWindow(display), 
                                10, 10, 400, 300, 1,
                                BlackPixel(display, DefaultScreen(display)),
                                WhitePixel(display, DefaultScreen(display)));

    // Map the window
    XMapWindow(display, window);
    
    // Flush the output buffer
    XFlush(display);

    // Change the window name (XStoreName / XInternAtom)
    XStoreName(display, window, "Intercepted Test Window");
    Atom myAtom = XInternAtom(display, "MY_TEST_ATOM", False);
    fprintf(stderr, "Got atom: %lu\n", (unsigned long)myAtom);

    // Move and resize the window
    XMoveWindow(display, window, 50, 50);
    XResizeWindow(display, window, 320, 240);

    // Raise and lower the window
    XRaiseWindow(display, window);
    sleep(1);
    XLowerWindow(display, window);

    // Change a property on the window
    const char *prop_val = "hello";
    XChangeProperty(display, window, XInternAtom(display, "MY_PROP", False), XA_STRING, 8, PropModeReplace, (const unsigned char*)prop_val, strlen(prop_val));

    // Get the atom name back
    char *atom_name = XGetAtomName(display, myAtom);
    if (atom_name) {
        fprintf(stderr, "Atom name: %s\n", atom_name);
    }

    // Get window attributes
    XWindowAttributes wa;
    if (XGetWindowAttributes(display, window, &wa)) {
        fprintf(stderr, "Window attrs: x=%d y=%d w=%u h=%u\n", wa.x, wa.y, wa.width, wa.height);
    }

    // Select input for the window
    XSelectInput(display, window, ExposureMask | KeyPressMask);

    // Create and free a pixmap
    Pixmap pm = XCreatePixmap(display, DefaultRootWindow(display), 16, 16, DefaultDepth(display, DefaultScreen(display)));
    fprintf(stderr, "Created pixmap: %lu\n", (unsigned long)pm);
    XFreePixmap(display, pm);

    // Send a simple ClientMessage to the window
    XClientMessageEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ClientMessage;
    ev.window = window;
    ev.message_type = XInternAtom(display, "MY_TEST_ATOM", False);
    ev.format = 8;
    strncpy((char*)ev.data.b, "data", 20);
    XSendEvent(display, window, False, NoEventMask, (XEvent*)&ev);

    // Configure the window (move/resize via Configure)
    XWindowChanges changes;
    changes.x = 100;
    changes.y = 100;
    changes.width = 200;
    changes.height = 150;
    XConfigureWindow(display, window, CWX | CWY | CWWidth | CWHeight, &changes);

    // Wait for a moment to see the window
    sleep(2);
    
    // Close the display
    XCloseDisplay(display);
    
    return 0;
}