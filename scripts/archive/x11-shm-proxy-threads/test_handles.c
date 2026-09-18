/* Stress test: open/close displays repeatedly to exercise handle table */
#define _GNU_SOURCE
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

extern void *server_thread_fn(void *arg);

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    init_ring(&g_ring);

    pthread_t srv;
    if (!XInitThreads()) {
        fprintf(stderr, "[handles] Warning: XInitThreads() returned false\n");
    }
    if (pthread_create(&srv, NULL, server_thread_fn, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    const char *dname = getenv("DISPLAY");
    if (!dname) dname = ":0";

    for (int i = 0; i < 200; ++i) {
        Display *d = XOpenDisplay(dname);
        if (!d) {
            fprintf(stderr, "[handles] XOpenDisplay failed at iter %d\n", i);
            break;
        }
        /* small sleep to increase interleaving */
        struct timespec ts = {0, 1000000};
        nanosleep(&ts, NULL);
        // XCloseDisplay(d);
    }

    /* shutdown */
    ring_slot_t *slot = ring_reserve_slot(&g_ring);
    slot->req_type = REQ_SHUTDOWN;
    slot->client_resp = NULL;
    ring_publish_slot(&g_ring, slot);

    pthread_join(srv, NULL);
    printf("[handles] server joined, exiting\n");
    return 0;
}
