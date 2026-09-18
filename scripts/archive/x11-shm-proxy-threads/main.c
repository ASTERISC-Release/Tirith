#define _GNU_SOURCE
#include "common.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include <string.h>

/* prototypes */
void *server_thread_fn(void *arg);
Display *XOpenDisplay(const char *display_name);
int XCloseDisplay(Display *display);
Window XCreateWindow(Display *dpy, Window parent, int x, int y, unsigned int width, unsigned int height,
                     unsigned int border_width, int depth, unsigned int klass, Visual *visual,
                     unsigned long valuemask, XSetWindowAttributes *attributes);
int XMapWindow(Display *dpy, Window w);

int main(void) {
    pthread_t srv;

    /* Make Xlib thread-safe before any Xlib calls or threads */
    if (!XInitThreads()) {
        fprintf(stderr, "[main] Warning: XInitThreads() returned false (Xlib may not be thread-safe)\n");
    }

    init_ring(&g_ring);

    if (pthread_create(&srv, NULL, server_thread_fn, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    const char *dname = getenv("DISPLAY");
    if (!dname) dname = ":0";

    printf("[main] XOpenDisplay('%s')\n", dname);
    Display *dpy = XOpenDisplay(dname);
    if (!dpy) {
        printf("[main] ❌ XOpenDisplay failed\n");
    } else {
        printf("[main] ✅ XOpenDisplay succeeded: %p\n", (void *)dpy);

    /* simple window create/map/destroy test */
    /* Use wrapper calls for root and white pixel so client only sees opaque handles */
    XSetWindowAttributes attrs;
    attrs.background_pixel = XWhitePixel(dpy, 0);
        attrs.event_mask = ExposureMask | StructureNotifyMask;

    /* request root window via wrapper */
    Window root = XRootWindow(dpy, 0);

    /* Pass NULL visual (server will use CopyFromParent behavior) */
    Window w = XCreateWindow(dpy, root, 20, 20, 200, 120, 0,
                 CopyFromParent, InputOutput, NULL,
                 CWBackPixel | CWEventMask, &attrs);
        if (!w) {
            printf("[main] ❌ XCreateWindow failed\n");
        } else {
            printf("[main] ✅ XCreateWindow succeeded: 0x%lx\n", (unsigned long)w);

            if (XMapWindow(dpy, w)) {
                printf("[main] ✅ XMapWindow succeeded\n");
            } else {
                printf("[main] ❌ XMapWindow failed\n");
            }

            sleep(1);

            /* demo: intern atom and get window attributes */
            Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
            printf("[main] XInternAtom(WM_PROTOCOLS) -> %lu\n", (unsigned long)wm_protocols);

            XWindowAttributes attrs;
            Status s = XGetWindowAttributes(dpy, w, &attrs);
            if (s) {
                printf("[main] XGetWindowAttributes: x=%d y=%d w=%u h=%u\n", attrs.x, attrs.y, attrs.width, attrs.height);
            } else {
                printf("[main] XGetWindowAttributes failed\n");
            }

            /* flush and sync */
            XFlush(dpy);
            XSync(dpy, False);

            /* destroy locally with XDestroyWindow? we didn't add wrapper for destroy in this step.
               Use XDestroyWindow local call (same-process) or add wrapper similarly. */
            XDestroyWindow(dpy, w);
        }


        // if (XCloseDisplay(dpy)) {
        //     printf("[main] ✅ XCloseDisplay succeeded\n");
        // } else {
        //     printf("[main] ❌ XCloseDisplay failed\n");
        // }
    }

    /* shutdown request */
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_SHUTDOWN;
    ClientDisplayResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);

    pthread_join(srv, NULL);
    printf("[main] done\n");
    return 0;
}
