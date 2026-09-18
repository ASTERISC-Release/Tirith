#pragma once
#ifndef _BYPASS_EGL_H
#define _BYPASS_EGL_H
#endif
#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/sync.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glext.h>


extern PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
extern PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ptr ;
extern PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ptr ;
extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ptr ;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_ptr;
extern PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT_ptr ;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ptr ;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ptr ;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ptr ;
extern PFNGLFENCESYNCPROC glFenceSync_ptr ;
extern PFNGLCLIENTWAITSYNCPROC glClientWaitSync_ptr ;
extern PFNGLDELETESYNCPROC glDeleteSync_ptr ;
extern PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUF_ptr ;
#define NUM_BUFFERS 3
#define XCB_DRI3_PIXMAP_SCANOUT (1<<0)
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ptr ;
typedef void (*PFNGLCLIPCONTROLPROC)(GLenum origin, GLenum depth);
extern PFNGLCLIPCONTROLPROC glClipControl_ptr;

extern struct gbm_device *gbm;
// #define WIDTH 1920
// #define HEIGHT 1080

#define WIDTH 300
#define HEIGHT 300

#define COMM_ADDR  0xf00000ULL
// #define COMM_ADDR  0x7FFFF000ULL
#define COMM_MAGIC 0x1234567812345678ULL
#define SYS_COMMS_ADDR 0xf00000ULL
// #define SYS_COMMS_ADDR 0x7FFFF000ULL
#define SYS_COMMS_SIZE 4096 // One page width.
static const  size_t FIVETWELVE_MEGABYTE = 1024*1024*512;
// static const  size_t DATA_SIZE = FIVETWELVE_MEGABYTE; // 1G
// static const  size_t DATA_SIZE = FIVETWELVE_MEGABYTE; // 1G
// static const  size_t DATA_SIZE = FIVETWELVE_MEGABYTE*2; // 1G
// static const  size_t DATA_SIZE = FIVETWELVE_MEGABYTE*2; // 1G
static const  size_t DATA_SIZE = FIVETWELVE_MEGABYTE; // 1G
// static void* DATA_REGION = (void*)0x100008000ULL;
static void* DATA_REGION = (void*)0x100000000ULL;
static void* HUGEPAGE_DATA_REGION = (void*)0x200000000ULL;
// static void* DATA_REGION = (void*)0x40000000ULL;


static const  uint64_t X11_SETUP = 12;
static const  uint64_t X11_PRESENT = 13;

/* Syscall logging toggles */
static const  uint64_t SYSCALL_LOGGING_ENABLE = 15;
static const  uint64_t SYSCALL_LOGGING_DISABLE = 16;

    /* Per-buffer state */
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
        GLuint rbo_depth;
    } check;
extern check bufs[NUM_BUFFERS];

typedef struct {
    volatile uint64_t magic;
    volatile uint64_t req_bit;
    volatile uint64_t p1;
    volatile uint64_t p2;
    volatile uint64_t p3;
    volatile uint64_t p4;
    volatile uint64_t p5;
    volatile uint64_t p6;
    volatile uint64_t p7;
    volatile uint64_t p8;
    volatile uint64_t p9;
    volatile uint64_t p10;
    volatile uint64_t ret;
} comm_page_t;


static inline comm_page_t* comm_page(void) {
    return (comm_page_t*)(uintptr_t)COMM_ADDR;
}

static uint64_t comm_sync_notify(comm_page_t* c) {
    if (!c || c->magic != COMM_MAGIC) {
        return 0;
    }
    // Busy-wait for host to clear req
    for (;;) {
        if (c->req_bit == 0) break;
    }

    return c->ret;
}

static void enable_syscall_logging(void) {
    comm_page_t* c = comm_page();
    c->req_bit = SYSCALL_LOGGING_ENABLE;
    comm_sync_notify(c);
}

static void disable_syscall_logging(void) {
    comm_page_t* c = comm_page();
    c->req_bit = SYSCALL_LOGGING_DISABLE;
    comm_sync_notify(c);
}

extern int cur;
extern GLuint fbo;
static void check_egl_error(const char *where) {
    EGLint e = eglGetError();
    if (e != EGL_SUCCESS) fprintf(stderr, "EGL error at %s: 0x%04x\n", where, e);
}

#define GL_UPPER_LEFT 0x8CA2
#define GL_ZERO_TO_ONE 0x935F

extern EGLDisplay eglDpy;

typedef void *(*dlsym_fn_t)(void *, const char *);
extern dlsym_fn_t real_dlsym ;

typedef void (*glXSwapBuffers_t)(Display *, GLXDrawable);
extern glXSwapBuffers_t real_glXSwapBuffers ;
extern xcb_sync_fence_t prev_present_fence;

extern int setup_egl();
