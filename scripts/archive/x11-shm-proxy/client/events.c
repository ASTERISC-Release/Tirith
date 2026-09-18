/* events.c --- event handling for client --- */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "common.h"

/* runtime pointers */
extern shared_region_t *g;
extern pid_t mypid;
extern uint32_t seq_counter;


/* synchronous RPC: write request and wait for response using req_mtx/req_cond */
int send_request_and_wait(ipc_slot_t *req, ipc_slot_t *resp_out) {
    // if (!ipc_ready()) return -1;
    req->pid = mypid;
    req->seq = __sync_fetch_and_add(&seq_counter, 1);

    /* write request under req_mtx */
    if (pthread_mutex_lock(&g->req_mtx) != 0) return -1;
    memcpy(&g->rpc_slot, req, sizeof(*req));
    g->req_ready = 1;
    g->resp_ready = 0;
    pthread_cond_signal(&g->req_cond);
    /* wait for server to set resp_ready */
    while (!g->resp_ready) {
        pthread_cond_wait(&g->req_cond, &g->req_mtx);
    }
    memcpy(resp_out, &g->rpc_slot, sizeof(*resp_out));
    /* reset flags */
    g->resp_ready = 0;
    g->req_ready = 0;
    pthread_mutex_unlock(&g->req_mtx);
    return 0;
}

/* event helpers */
int event_count(void) {
    if (!g) return 0;
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return 0;
    uint32_t h = g->evt_ring.head, t = g->evt_ring.tail, c = g->evt_ring.cap;
    int cnt = (t + c - h) % c;
    pthread_mutex_unlock(&g->evt_mtx);
    return cnt;
}

/* pop next event (blocks until available) */
int pop_event(XEvent *out) {
    if (!g) return -1;
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return -1;
    while (g->evt_ring.head == g->evt_ring.tail) {
        pthread_cond_wait(&g->evt_cond, &g->evt_mtx);
    }
    *out = g->evt_ring.events[g->evt_ring.head];
    g->evt_ring.head = (g->evt_ring.head + 1) % g->evt_ring.cap;
    pthread_mutex_unlock(&g->evt_mtx);
    return 0;
}
