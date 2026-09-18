#pragma once
#ifndef QEMU_SG_H
#define QEMU_SG_H

#define NUM_COMM_PAGES 1

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <EGL/eglext.h>
#include <xcb/xcb.h>
#include <xcb/sync.h>

/* Uncomment to enable debugging */
// #define COMMAND_DEBUG
// #define GEM_DEBUG
// #define STAT_DEBUG

/* Syscall logging toggle */
extern int syscall_logging_enabled;

static void set_syscall_logging(int enable) { syscall_logging_enabled = enable ? 1 : 0; }

#ifdef COMMAND_DEBUG
#define log_sg(fmt, ...) \
    do { \
        if (syscall_logging_enabled) { \
            fprintf(stderr, "(qemu: COMMAND) "); \
            fprintf(stderr, fmt, ##__VA_ARGS__); \
        } \
    } while (0)
#else
#define log_sg(fmt, ...) do {} while (0)
#endif

#ifdef GEM_DEBUG
#define log_gem(fmt, ...) \
    do { \
        fprintf(stderr, "(qemu: GEM) "); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
    } while (0)
#else
#define log_gem(fmt, ...) do {} while (0)
#endif

#ifdef STAT_DEBUG
#define log_stat(fmt, ...) \
    do { \
        fprintf(stderr, "(qemu: STAT) "); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
    } while (0)
#else
#define log_stat(fmt, ...) do {} while (0)
#endif

#define log_always(fmt, ...) \
    do { \
        fprintf(stderr, "(qemu) "); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
    } while (0)

/* sg-listener.c */
extern xcb_window_t win;
extern xcb_connection_t *conn;
void set_presentation_window(xcb_window_t window);
void set_shared_input_mask(xcb_window_t window, uint32_t event_mask);
int get_guest_tsc_info(uint64_t *offset, uint32_t *frequency_khz);

extern void* data_region_actual_address;
extern void* global_ram_address;
extern void* global_ram1_address;
extern void* global_ram2_address;
extern void* global_ram3_address;
typedef struct {
    volatile uint64_t magic;
    volatile uint64_t req_bit;
    volatile uint64_t ret;
    volatile uint64_t p1;
    volatile uint64_t p2;
    volatile uint64_t p3;
    volatile uint64_t p4;
    volatile uint64_t p5;
} comm_page_t;

_Static_assert(offsetof(comm_page_t, ret) == 16, "SG response must stay in the hot cache line");
_Static_assert(offsetof(comm_page_t, p5) == 56, "common SG payload must fit one cache line");
_Static_assert(sizeof(comm_page_t) == 64, "SG communication ABI must fit one cache line");

typedef struct buffer {
    struct gbm_bo *bo;
    int bo_fd;
    xcb_pixmap_t pixmap;
    EGLImageKHR image;
    GLuint tex;
    GLuint fbo;
    int shm_fence_fd;
    struct xshmfence *shm_fence;
    xcb_sync_fence_t sync_fence;
    int idle_fence_fd;
    struct xshmfence *idle_fence;
    xcb_sync_fence_t idle_sync_fence;
    GLuint rbo_depth;
} check;

typedef enum RequestType {
    LOG_MMAP_EVENT = 1,
    SETUP_DATA = 2,
    GEM_ALLOCATION = 3,
    FSTAT = 4,
    IOCTL = 5,
    OPEN = 6,
    FCNTL = 7,
    READLINK = 8,
    NEWFSTAT = 9,
    GETDENT = 10,
    DUP = 11,
    DUP_IDENTITY = 30,
    X11_SETUP = 12,
    X11_PRESENT = 13,
    X11_IDLE_FENCE_BUFFER = 33,
    CLOSE = 14,
    SYSCALL_LOGGING_ENABLE = 15,
    SYSCALL_LOGGING_DISABLE = 16,
    LSEEK = 22,
    UPDATE_WINDOW_SIZE = 23,
    READ = 24,
    WRITE = 25,
    SYSV_SEMGET = 34,
    SYSV_SEMCTL = 35,
    SYSV_SEMOP = 36,
    HOST_FLOCK = 37,
    HOST_FTRUNCATE = 38,
    UPDATE_INPUT_MASK = 39,
} RequestType;

// Sizes
static const size_t THREE_GIGABYTES = 3ULL * 1024 * 1024 * 1024;
static const size_t ONE_GIGABYTE = 1ULL * 1024 * 1024 * 1024;
/* The regular arena occupies [4GiB, 7GiB), and the hugepage arena occupies [8GiB, 9GiB).
 * A 16GiB VM still leaves ample space above both ranges for LibOS and application mappings. */
static const size_t DATA_SIZE      = THREE_GIGABYTES;
static const size_t HUGE_DATA_SIZE = ONE_GIGABYTE;
static const  size_t PAGE_SIZE    = 4*1024;

#define COMM_ADDR  0xf00000ULL
#define COMM_MAGIC 0x1234567812345678ULL

// static void* DATA_REGION = (void*)0x100008000ULL;
static void* DATA_REGION = (void*)0x100000000ULL;
static void* HUGEPAGE_DATA_REGION = (void*)0x200000000ULL;
static void* DATA_HOST_OFFSET = (void*)0x80000000ULL;

// This is different than where it appears in the guest.
// Qemu maps the ram region from 0x80000000 into the allocation to 0x100000000 in the guest AS
// static void* DATA_HOST_OFFSET = (void*)0x80008000ULL;

static void* UNMAP_DATA_MSG = (void*)0x1234567f1234567fULL;


void* mmap_listener(void* arg);

// #define WIDTH 1280
// #define HEIGHT 720
extern int win_width;
extern int win_height;

#define NUM_BUFFERS 3

#define sys_exec_vmexits 549
#define sys_sg_vmexits_printreset 550
#define I915_EXEC_ASYNC (1<<15)
#define LOG_BATCH_SIZE 100  // Number of entries to hold in memory before flushing

static inline uint64_t clock_gettime_ns(void)
{
    unsigned int lo, hi;
    asm volatile("lfence; rdtscp" : "=a"(lo), "=d"(hi) :: "memory");
    return ((uint64_t)hi << 32) | lo;
}

// Structure to hold our raw measurements
typedef struct {
    uint64_t req_type;
    int frame;
    uint64_t cycles;
    int ret;
    uint64_t flags;
} log_entry_t;

static void prefault_range(void *addr, size_t len) {
    char *p = addr;
    for (size_t off = 0; off < len; off += PAGE_SIZE)
        memset((void*)(p + off), 0, PAGE_SIZE);
}
#endif
