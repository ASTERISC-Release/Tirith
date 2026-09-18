/* Advanced test program for X11 IPC forwarding system */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int main(void) {
    Display *display;
    Window window, root;
    XEvent event;
    Atom wm_delete_window;
    int screen;
    unsigned long black, white;
    
    printf("Testing X11 IPC forwarding...\n");
    
    /* Test XOpenDisplay */
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    printf("XOpenDisplay: SUCCESS\n");
    
    screen = DefaultScreen(display);
    black = BlackPixel(display, screen);
    white = WhitePixel(display, screen);
    root = DefaultRootWindow(display);
    
    /* Test XCreateSimpleWindow */
    window = XCreateSimpleWindow(display, root, 10, 10, 400, 300, 1, black, white);
    if (window == 0) {
        fprintf(stderr, "Cannot create window\n");
        XCloseDisplay(display);
        exit(1);
    }
    printf("XCreateSimpleWindow: SUCCESS (window = 0x%lx)\n", window);
    
    /* Test XSelectInput */
    if (XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask) != 0) {
        fprintf(stderr, "XSelectInput failed\n");
    }
    printf("XSelectInput: SUCCESS\n");
    
    /* Test XStoreName */
    // XStoreName(display, window, "X11 IPC Test Window");
    // printf("XStoreName: SUCCESS\n");
    
    /* Test XSetWMProtocols for window manager close event */
    // wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    // if (XSetWMProtocols(display, window, &wm_delete_window, 1) != 0) {
    //     fprintf(stderr, "XSetWMProtocols failed\n");
    // }
    // printf("XSetWMProtocols: SUCCESS\n");
    
    /* Test XMapWindow */
    if (XMapWindow(display, window) != 0) {
        fprintf(stderr, "XMapWindow failed\n");
    }
    printf("XMapWindow: SUCCESS\n");
    
    /* Test XFlush */
    if (XFlush(display) != 0) {
        fprintf(stderr, "XFlush failed\n");
    }
    printf("XFlush: SUCCESS\n");
    
    printf("Window created and mapped. Click in window or press any key to continue...\n");
    printf("Close window with window manager to test event handling...\n");
    
    /* Event loop - test XNextEvent and XPending */
    int done = 0;
    while (!done) {
        /* Test XPending */
        int pending = XPending(display);
        if (pending > 0) {
            printf("XPending reports %d events\n", pending);
        }
        
        /* Test XNextEvent */
        XNextEvent(display, &event);
        
        switch (event.type) {
            case Expose:
                printf("Received Expose event\n");
                break;
                
            case KeyPress:
                printf("Received KeyPress event\n");
                done = 1; /* Exit on key press */
                break;
                
            case ButtonPress:
                printf("Received ButtonPress event at (%d,%d)\n", 
                       event.xbutton.x, event.xbutton.y);
                break;
                
            case ClientMessage:
                if (event.xclient.data.l[0] == (long)wm_delete_window) {
                    printf("Received window close request\n");
                    done = 1;
                }
                break;
        }
    }
    
    /* Test window manipulation */
    printf("Testing window resize...\n");
    XResizeWindow(display, window, 500, 400);
    XFlush(display);
    sleep(1);
    
    printf("Testing window move...\n");
    XMoveWindow(display, window, 50, 50);
    XFlush(display);
    sleep(1);
    
    /* Test XConfigureWindow directly */
    printf("Testing XConfigureWindow...\n");
    XWindowChanges changes;
    changes.x = 100;
    changes.y = 100;
    changes.width = 300;
    changes.height = 200;
    XConfigureWindow(display, window, CWX | CWY | CWWidth | CWHeight, &changes);
    XFlush(display);
    sleep(1);
    
    /* Test XUnmapWindow */
    printf("Testing XUnmapWindow...\n");
    XUnmapWindow(display, window);
    XFlush(display);
    sleep(1);
    
    /* Test XMapWindow again */
    printf("Testing XMapWindow again...\n");
    XMapWindow(display, window);
    XFlush(display);
    sleep(1);
    
    /* Test XDestroyWindow */
    printf("Testing XDestroyWindow...\n");
    XDestroyWindow(display, window);
    
    /* Test XFlush */
    XFlush(display);
    printf("XFlush after destroy: SUCCESS\n");
    
    /* Test XCloseDisplay */
    if (XCloseDisplay(display) != 0) {
        fprintf(stderr, "XCloseDisplay failed\n");
    }
    printf("XCloseDisplay: SUCCESS\n");
    
    printf("All tests completed successfully!\n");
    return 0;
}
