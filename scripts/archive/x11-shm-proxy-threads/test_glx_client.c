/* Test: obtain real Display* from server and call glXCreateContext locally */
#define _GNU_SOURCE
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <GL/glx.h>
#include <string.h>
#include <pthread.h>

/* prototypes from the project */
extern void *server_thread_fn(void *arg);

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    init_ring(&g_ring);
    pthread_t srv;
    /* Make Xlib thread-safe before any Xlib calls or threads */
    if (!XInitThreads()) {
        fprintf(stderr, "[test] Warning: XInitThreads() returned false (Xlib may not be thread-safe)\n");
    }
    printf("[test] starting server thread\n");
    if (pthread_create(&srv, NULL, server_thread_fn, NULL) != 0) {
        perror("pthread_create");
        return 2;
    }
    printf("[test] server thread started: %p\n", (void*) (size_t) srv);

    const char *dname = getenv("DISPLAY");
    if (!dname) dname = ":0";

    printf("[test] calling XOpenDisplay('%s')\n", dname);
    Display *fake = XOpenDisplay(dname);
    printf("[test] XOpenDisplay returned fake=%p\n", (void*)fake);
    if (!fake) {
        printf("[test] XOpenDisplay failed\n");
        return 1;
    }

    printf("[test] requesting real Display* for fake=%p\n", (void*)fake);
    Display *real = XGetRealDisplay(fake);
    printf("[test] XGetRealDisplay returned real=%p\n", (void*)real);
    if (!real) {
        printf("[test] XGetRealDisplay failed\n");
        return 1;
    }

    int screen = DefaultScreen(real);
    int attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    printf("[test] calling glXChooseVisual on real=%p screen=%d\n", (void*)real, screen);
    XVisualInfo *vis = glXChooseVisual(real, screen, attribs);
    printf("[test] glXChooseVisual returned vis=%p\n", (void*)vis);
    if (!vis) {
        printf("[test] glXChooseVisual failed\n");
    }
    printf("[test] calling glXCreateContext\n");
    GLXContext ctx = glXCreateContext(fake, vis, NULL, True);
    printf("[test] glXCreateContext returned ctx=%p\n", (void*)ctx);
    if (!ctx) {
        printf("[test] glXCreateContext failed\n");
    } else {
        printf("[test] glXCreateContext succeeded: %p\n", (void*)ctx);
        GLXContext cur = glXGetCurrentContext();
        printf("[test] glXGetCurrentContext -> %p\n", (void*)cur);
        printf("[test] calling glXDestroyContext with fake=%p ctx=%p\n", (void*)fake, (void*)ctx);
        glXDestroyContext(fake, ctx);
        printf("[test] glXDestroyContext returned\n");
    }

    printf("[test] calling XCloseDisplay(fake=%p)\n", (void*)fake);
    // XCloseDisplay(fake);

    /* shutdown */
    printf("[test] sending REQ_SHUTDOWN\n");
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_SHUTDOWN;
    ClientDisplayResp resp;
    memset(&resp, 0, sizeof(resp));
    slot->client_resp = &resp;
    ring_publish_slot(&g_ring, slot);
    slot_wait_completed(slot);
    printf("[test] shutdown completed, joining server thread\n");
    pthread_join(srv, NULL);
    printf("[test] server thread joined\n");
    return 0;
}
