#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    Display *display;
    int screen;
    Window root_window, window;
    XEvent event;
    int bigreq_event, bigreq_error;
    long max_request_size;
    
    printf("Connecting to X11 server...\n");
    
    // Connect to X server
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    
    printf("Connected to X11 server\n");
    
    screen = DefaultScreen(display);
    root_window = RootWindow(display, screen);
    
    // Test 1: Query BIG-REQUESTS extension
    printf("Testing BIG-REQUESTS extension...\n");
    if (XQueryExtension(display, "BIG-REQUESTS", &bigreq_event, &bigreq_error, &bigreq_event)) {
        printf("BIG-REQUESTS extension found\n");
        
        // Enable BIG-REQUESTS
        max_request_size = XExtendedMaxRequestSize(display);
        if (max_request_size > 0) {
            printf("Max request size: %ld bytes\n", max_request_size);
        } else {
            printf("BIG-REQUESTS not supported or disabled\n");
        }
    } else {
        printf("BIG-REQUESTS extension not found\n");
    }
    
    // Test 2: Multiple X11 requests
    printf("Making multiple X11 requests...\n");
    for (int i = 0; i < 10; i++) {
        int width = DisplayWidth(display, screen);
        int height = DisplayHeight(display, screen);
        printf("Display size test %d: %dx%d\n", i, width, height);
    }
    
    // Test 3: Create and destroy windows
    printf("Testing window operations...\n");
    for (int i = 0; i < 5; i++) {
        window = XCreateSimpleWindow(display, root_window, 10, 10, 200, 200, 1,
                                   BlackPixel(display, screen),
                                   WhitePixel(display, screen));
        
        if (window) {
            printf("Window %d created successfully\n", i);
            XDestroyWindow(display, window);
            printf("Window %d destroyed\n", i);
        } else {
            printf("Failed to create window %d\n", i);
        }
    }
    
    // Test 4: Query extensions
    printf("Querying extensions...\n");
    int nextensions = 0;
    char **extensions = XListExtensions(display, &nextensions);
    if (extensions) {
        printf("Available extensions (%d):\n", nextensions);
        for (int i = 0; i < nextensions && i < 10; i++) {  // Show first 10
            printf("  %s\n", extensions[i]);
        }
        XFreeExtensionList(extensions);
    }
    
    // Test 5: Flush and sync
    printf("Flushing and syncing...\n");
    XFlush(display);
    XSync(display, False);
    
    // Close connection
    XCloseDisplay(display);
    printf("X11 connection closed successfully\n");
    
    return 0;
}
