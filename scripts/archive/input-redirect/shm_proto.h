// shm_proto.h
#pragma once
#include <pthread.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#define SHM_NAME "/sdl_input_rpc_shm_v1"
#define MAGIC_COOKIE 0x53444C49  // 'SDLI'

typedef enum { REQ_POLL=1, REQ_WAIT=2, REQ_SHUTDOWN=3 } req_type_t;

/* client writes this request (single-slot) */
typedef struct {
    uint32_t cookie;
    uint32_t req_id;      // monotonic id
    uint32_t req_type;    // req_type_t
    uint32_t padding;
} shm_request_t;

/* server writes this response */
typedef struct {
    uint32_t cookie;
    uint32_t resp_id;     // mirrors req_id
    uint32_t status;      // 0=ok, 1=timeout, 2=shutdown
    uint32_t event_size;  // bytes used in event_blob
    uint8_t  event_blob[512]; // should fit SDL_Event for supported events
} shm_response_t;

/* whole shared region */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond_client; // client waits on this for response
    pthread_cond_t  cond_server; // server waits on this for request
    shm_request_t   req;
    shm_response_t  resp;
} shm_region_t;

