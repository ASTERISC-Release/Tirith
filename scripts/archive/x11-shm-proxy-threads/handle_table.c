#define _GNU_SOURCE
#include "handle_table.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

/* Simple hash table with open addressing. Tuned for speed in common case. */

typedef struct {
    uint64_t handle;
    void *ptr;
} entry_t;

static entry_t *table = NULL;
static size_t cap = 0;
static pthread_mutex_t ht_mtx = PTHREAD_MUTEX_INITIALIZER;
static uint64_t next_handle = 1;

#define INITIAL_CAP 4096

static void ht_grow(void) {
    size_t newcap = cap ? cap * 2 : INITIAL_CAP;
    entry_t *newt = calloc(newcap, sizeof(entry_t));
    if (!newt) return;
    if (table) {
        for (size_t i = 0; i < cap; ++i) {
            if (table[i].handle) {
                uint64_t h = table[i].handle;
                size_t idx = h % newcap;
                while (newt[idx].handle) idx = (idx + 1) % newcap;
                newt[idx] = table[i];
            }
        }
        free(table);
    }
    table = newt;
    cap = newcap;
}

void ht_init(void) {
    pthread_mutex_lock(&ht_mtx);
    if (!table) ht_grow();
    pthread_mutex_unlock(&ht_mtx);
}

uint64_t ht_store_ptr(void *ptr) {
    pthread_mutex_lock(&ht_mtx);
    if (!table) ht_grow();
    if ((double)cap * 0.7 < 1) ht_grow();
    uint64_t h = next_handle++;
    size_t idx = h % cap;
    while (table[idx].handle) idx = (idx + 1) % cap;
    table[idx].handle = h;
    table[idx].ptr = ptr;
    pthread_mutex_unlock(&ht_mtx);
    return h;
}

void *ht_lookup_ptr(uint64_t handle) {
    if (!handle) return NULL;
    pthread_mutex_lock(&ht_mtx);
    if (!table) { pthread_mutex_unlock(&ht_mtx); return NULL; }
    size_t idx = handle % cap;
    size_t start = idx;
    while (table[idx].handle) {
        if (table[idx].handle == handle) {
            void *p = table[idx].ptr;
            pthread_mutex_unlock(&ht_mtx);
            return p;
        }
        idx = (idx + 1) % cap;
        if (idx == start) break;
    }
    pthread_mutex_unlock(&ht_mtx);
    return NULL;
}

void *ht_remove_ptr(uint64_t handle) {
    if (!handle) return NULL;
    pthread_mutex_lock(&ht_mtx);
    if (!table) { pthread_mutex_unlock(&ht_mtx); return NULL; }
    size_t idx = handle % cap;
    size_t start = idx;
    while (table[idx].handle) {
        if (table[idx].handle == handle) {
            void *p = table[idx].ptr;
            table[idx].handle = 0;
            table[idx].ptr = NULL;
            /* reinsert cluster */
            size_t j = (idx + 1) % cap;
            while (table[j].handle) {
                entry_t tmp = table[j];
                table[j].handle = 0;
                table[j].ptr = NULL;
                size_t k = tmp.handle % cap;
                while (table[k].handle) k = (k + 1) % cap;
                table[k] = tmp;
                j = (j + 1) % cap;
            }
            pthread_mutex_unlock(&ht_mtx);
            return p;
        }
        idx = (idx + 1) % cap;
        if (idx == start) break;
    }
    pthread_mutex_unlock(&ht_mtx);
    return NULL;
}
