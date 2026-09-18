// client_stub.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "shm_proto.h"

static shm_region_t *g_shm = NULL;
static uint32_t g_next_req = 1;
static int connect_shm_once(void) {
    if (g_shm) return 0;
    int fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (fd < 0) { return -1; }
    void *mem = mmap(NULL, sizeof(shm_region_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (mem == MAP_FAILED) { return -1; }
    g_shm = (shm_region_t*)mem;
    // optionally verify cookie
    return 0;
}

static int wait_for_response(uint32_t req_id, shm_response_t *out, int timeout_ms) {
    int rc = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000; }

    pthread_mutex_lock(&g_shm->mutex);
    while (g_shm->resp.resp_id != req_id) {
        int r = pthread_cond_timedwait(&g_shm->cond_client, &g_shm->mutex, &ts);
        if (r == ETIMEDOUT) { rc = -1; break; }
    }
    if (rc == 0) memcpy(out, &g_shm->resp, sizeof(*out));
    pthread_mutex_unlock(&g_shm->mutex);
    return rc;
}

/* wrapper for SDL_PollEvent */
int SDL_PollEvent(SDL_Event *event) {
    typedef int (*orig_poll_fn)(SDL_Event*);
    static orig_poll_fn orig = NULL;
    if (!orig) orig = (orig_poll_fn)dlsym(RTLD_NEXT, "SDL_PollEvent");

    if (connect_shm_once() != 0) {
        // server not available: fallback to original SDL
        return orig ? orig(event) : 0;
    }

    // prepare request
    pthread_mutex_lock(&g_shm->mutex);
    uint32_t reqid = g_next_req++;
    g_shm->req.cookie = MAGIC_COOKIE;
    g_shm->req.req_id = reqid;
    g_shm->req.req_type = REQ_POLL;
    // notify server
    pthread_cond_signal(&g_shm->cond_server);
    pthread_mutex_unlock(&g_shm->mutex);

    shm_response_t resp;
    int w = wait_for_response(reqid, &resp, 2000); // 2s timeout; tune as needed
    if (w != 0) {
        // timeout or error -> fallback to original SDL_PollEvent
        return orig ? orig(event) : 0;
    }
    if (resp.status != 0) {
        return 0; // no event
    }
    // deserialize SDL_Event from blob (we only handle a few event types)
    memset(event, 0, sizeof(*event));
    // event_blob contains raw SDL_Event; unsafe but okay when both use same SDL build
    if (resp.event_size <= sizeof(SDL_Event)) {
        memcpy(event, resp.event_blob, resp.event_size);
        return 1;
    } else {
        return 0;
    }
}

/* wrapper for SDL_WaitEvent (blocking) */
int SDL_WaitEvent(SDL_Event *event) {
    typedef int (*orig_wait_fn)(SDL_Event*);
    static orig_wait_fn orig = NULL;
    if (!orig) orig = (orig_wait_fn)dlsym(RTLD_NEXT, "SDL_WaitEvent");

    if (connect_shm_once() != 0) {
        return orig ? orig(event) : 0;
    }

    pthread_mutex_lock(&g_shm->mutex);
    uint32_t reqid = g_next_req++;
    g_shm->req.cookie = MAGIC_COOKIE;
    g_shm->req.req_id = reqid;
    g_shm->req.req_type = REQ_WAIT;
    pthread_cond_signal(&g_shm->cond_server);
    pthread_mutex_unlock(&g_shm->mutex);

    shm_response_t resp;
    int w = wait_for_response(reqid, &resp, 0x7fffffff); // effectively infinite; tune if desired
    if (w != 0) return orig ? orig(event) : 0;
    if (resp.status != 0) return 0;
    memset(event,0,sizeof(*event));
    if (resp.event_size <= sizeof(SDL_Event)) {
        memcpy(event, resp.event_blob, resp.event_size);
        return 1;
    } else return 0;
}

