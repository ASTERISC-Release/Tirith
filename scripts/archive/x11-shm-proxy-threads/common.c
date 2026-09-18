#define _GNU_SOURCE
#include "common.h"
#include <string.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

ring_t g_ring;

void init_ring(ring_t *r) {
    atomic_init(&r->head, 0);
    atomic_init(&r->tail, 0);
    atomic_init(&r->next_req_id, 1);

    for (size_t i = 0; i < RING_CAPACITY; ++i) {
        ring_slot_t *s = &r->slots[i];
        atomic_init(&s->seq, i);
        s->req_ticket = 0;
        s->req_id = 0;
        s->req_type = REQ_NONE;
        s->display_name_ptr = NULL;
        s->req_display_ptr = NULL;
        s->req_parent = 0;
        s->req_x = s->req_y = 0;
        s->req_width = s->req_height = 0;
        s->req_border_width = 0;
        s->req_depth = 0;
        s->req_class = 0;
        s->req_visual = NULL;
        s->req_valuemask = 0;
        s->req_attributes = NULL;
        s->client_resp = NULL;
        pthread_mutex_init(&s->wait_mutex, NULL);
        pthread_cond_init(&s->wait_cond, NULL);
        atomic_init(&s->completed, 0);
    }
}

/* Reserve slot (producer gets a ticket). This spins until slot seq == ticket. */
ring_slot_t *ring_reserve_slot(ring_t *r) {
    uint64_t ticket = atomic_fetch_add_explicit(&r->head, 1, memory_order_acq_rel);
    ring_slot_t *slot = &r->slots[ticket & RING_MASK];
    for (;;) {
        uint64_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
        if (seq == ticket) {
            slot->req_ticket = ticket;
            slot->req_id = atomic_fetch_add_explicit(&r->next_req_id, 1, memory_order_acq_rel);
            atomic_store_explicit(&slot->completed, 0, memory_order_release);
            return slot;
        }
        sched_yield();
    }
}

/* Publish: make slot visible to consumer by setting seq = ticket + 1 */
void ring_publish_slot(ring_t *r, ring_slot_t *slot) {
    uint64_t ticket = slot->req_ticket;
    atomic_store_explicit(&slot->seq, ticket + 1, memory_order_release);
}

/* Consumer: get next ready slot (busy-waits) */
ring_slot_t *ring_consume_slot(ring_t *r) {
    uint64_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    for (;;) {
        ring_slot_t *slot = &r->slots[tail & RING_MASK];
        uint64_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
        if (seq == tail + 1) {
            atomic_store_explicit(&r->tail, tail + 1, memory_order_release);
            return slot;
        }
        sched_yield();
        tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    }
}

/* Release slot after processing; allow reuse by setting seq = tail + RING_CAPACITY */
void ring_release_slot(ring_t *r, ring_slot_t *slot) {
    uint64_t next_seq = atomic_load_explicit(&r->tail, memory_order_acquire) + RING_CAPACITY;
    atomic_store_explicit(&slot->seq, next_seq, memory_order_release);
}

/* Per-slot wait/signal */
void slot_wait_completed(ring_slot_t *slot) {
    if (atomic_load_explicit(&slot->completed, memory_order_acquire)) return;
    pthread_mutex_lock(&slot->wait_mutex);
    while (!atomic_load_explicit(&slot->completed, memory_order_acquire)) {
        pthread_cond_wait(&slot->wait_cond, &slot->wait_mutex);
    }
    pthread_mutex_unlock(&slot->wait_mutex);
}

void slot_signal_completed(ring_slot_t *slot) {
    atomic_store_explicit(&slot->completed, 1, memory_order_release);
    pthread_mutex_lock(&slot->wait_mutex);
    pthread_cond_signal(&slot->wait_cond);
    pthread_mutex_unlock(&slot->wait_mutex);
}

int slot_timed_wait_completed(ring_slot_t *slot, long timeout_ms) {
    if (atomic_load_explicit(&slot->completed, memory_order_acquire)) return 0;

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return -1;
    /* add timeout_ms */
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    ts.tv_sec += timeout_ms / 1000 + ts.tv_nsec / 1000000000L;
    ts.tv_nsec %= 1000000000L;

    pthread_mutex_lock(&slot->wait_mutex);
    int rc = 0;
    while (!atomic_load_explicit(&slot->completed, memory_order_acquire)) {
        rc = pthread_cond_timedwait(&slot->wait_cond, &slot->wait_mutex, &ts);
        if (rc == ETIMEDOUT) break;
    }
    pthread_mutex_unlock(&slot->wait_mutex);
    if (rc == ETIMEDOUT) return -1;
    return 0;
}