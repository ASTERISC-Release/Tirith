#ifndef _BYPASS_EGL_H
#define _BYPASS_EGL_H

#define _GNU_SOURCE
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/sync.h>
#include <xcb/xcb.h>

#include <GL/gl.h>
#include <GL/glext.h>

extern PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
extern PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ptr;
extern PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ptr;
extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ptr;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_ptr;
extern PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT_ptr;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ptr;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ptr;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_ptr;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ptr;
extern PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers_ptr;
extern PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer_ptr;
extern PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage_ptr;
extern PFNGLFENCESYNCPROC glFenceSync_ptr;
extern PFNGLCLIENTWAITSYNCPROC glClientWaitSync_ptr;
extern PFNGLDELETESYNCPROC glDeleteSync_ptr;
extern PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUF_ptr;
#define NUM_BUFFERS 3
#define XCB_DRI3_PIXMAP_SCANOUT (1 << 0)
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ptr;
typedef void (*PFNGLCLIPCONTROLPROC)(GLenum origin, GLenum depth);
extern PFNGLCLIPCONTROLPROC glClipControl_ptr;

extern struct gbm_device *gbm;
#define DEFAULT_WIDTH 300
#define DEFAULT_HEIGHT 300

extern int win_width;
extern int win_height;

extern xcb_connection_t *conn;
extern xcb_window_t win;
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

extern int cur;
extern GLuint fbo;
static void check_egl_error(const char *where) {
  EGLint e = eglGetError();
  if (e != EGL_SUCCESS)
    fprintf(stderr, "EGL error at %s: 0x%04x\n", where, e);
}

#define GL_UPPER_LEFT 0x8CA2
#define GL_ZERO_TO_ONE 0x935F

extern EGLDisplay eglDpy;

typedef void *(*dlsym_fn_t)(void *, const char *);
extern dlsym_fn_t real_dlsym;

typedef void (*glXSwapBuffers_t)(Display *, GLXDrawable);
extern glXSwapBuffers_t real_glXSwapBuffers;
extern xcb_sync_fence_t prev_present_fence;
/* Syscall logging toggle (set via set_syscall_logging) */
extern int syscall_logging_enabled;
/* Toggle function to enable/disable syscall logging at runtime */
void set_syscall_logging(int enable);

#endif