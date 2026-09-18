#define _GNU_SOURCE
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <time.h>
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
#include <math.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/xfixes.h>

#include <X11/xshmfence.h>
#include <drm/drm.h>
#include <drm/drm_fourcc.h>
#include <drm/i915_drm.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "helpers/bypass-egl.h"
#include <X11/xshmfence.h>

/* helpers/bypass-egl.c */
void setup_egl(void);

/* Forward declaration of our hook to avoid implicit decls when called earlier
 */
void glBindFramebuffer(GLenum target, GLuint framebuffer);

PFNGLBINDFRAMEBUFFERPROC glClipControl_ptr = NULL;
PFNGLFENCESYNCPROC glFenceSync_ptr = NULL;
PFNGLDELETESYNCPROC glDeleteSync_ptr = NULL;
PFNGLCLIENTWAITSYNCPROC glClientWaitSync_ptr = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ptr;
dlsym_fn_t real_dlsym;
static uint64_t ioctl_count = 0;
static uint64_t ioctl_total_ns = 0;
static uint64_t ioctl_max_ns = 0;
static uint64_t frame_count = 0;
static bool ioctl_logging_enabled = false;
#define IOCTL_STATS_MAX 64
typedef struct ioctl_stat {
  unsigned long request;
  uint64_t count;
  uint64_t total_ns;
  uint64_t max_ns;
  uint64_t last_fd;
} ioctl_stat;

static ioctl_stat ioctl_stats[IOCTL_STATS_MAX];
static size_t ioctl_stats_used = 0;
static uint64_t execbuffer2_last_flags = 0;
static uint32_t syncobj_wait_last_flags = 0;

/* Syscall logging toggle */
int syscall_logging_enabled = 0;

void set_syscall_logging(int enable) { syscall_logging_enabled = enable ? 1 : 0; }

static void print_syncobj_wait_flags(FILE *out, uint32_t flags) {
  if (flags == 0) {
    fprintf(out, "NONE");
    return;
  }

  bool first = true;
#define PRINT_SYNCOBJ_FLAG(flag)                                            \
  do {                                                                       \
    if (flags & (flag)) {                                                    \
      fprintf(out, "%s%s", first ? "" : "|", #flag);                    \
      first = false;                                                         \
    }                                                                        \
  } while (0)

  PRINT_SYNCOBJ_FLAG(DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL);
  PRINT_SYNCOBJ_FLAG(DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT);

  if (first)
    fprintf(out, "0x%x", flags);

#undef PRINT_SYNCOBJ_FLAG
}

static void print_execbuffer2_flags(FILE *out, uint64_t flags) {
  if (flags == 0) {
    fprintf(out, "NONE");
    return;
  }

  bool first = true;
#define PRINT_FLAG(flag)                                                     \
  do {                                                                       \
    if (flags & (flag)) {                                                    \
      fprintf(out, "%s%s", first ? "" : "|", #flag);                     \
      first = false;                                                         \
    }                                                                        \
  } while (0)

  PRINT_FLAG(I915_EXEC_RING_MASK);
  PRINT_FLAG(I915_EXEC_DEFAULT);
  PRINT_FLAG(I915_EXEC_RENDER);
  PRINT_FLAG(I915_EXEC_BSD);
  PRINT_FLAG(I915_EXEC_BLT);
  PRINT_FLAG(I915_EXEC_VEBOX);
  PRINT_FLAG(I915_EXEC_SECURE);
  PRINT_FLAG(I915_EXEC_NO_RELOC);
  PRINT_FLAG(I915_EXEC_HANDLE_LUT);
  PRINT_FLAG(I915_EXEC_BSD_MASK);
  PRINT_FLAG(I915_EXEC_RESOURCE_STREAMER);
  PRINT_FLAG(I915_EXEC_FENCE_ARRAY);
  PRINT_FLAG(I915_EXEC_FENCE_OUT);
  PRINT_FLAG(I915_EXEC_USE_EXTENSIONS);
#ifdef I915_EXEC_NO_FENCE
  PRINT_FLAG(I915_EXEC_NO_FENCE);
#endif
  PRINT_FLAG(I915_EXEC_BATCH_FIRST);
  PRINT_FLAG(I915_EXEC_FENCE_SUBMIT);
#ifdef I915_EXEC_CAPTURE
  PRINT_FLAG(I915_EXEC_CAPTURE);
#endif
#ifdef I915_EXEC_DEBUG
  PRINT_FLAG(I915_EXEC_DEBUG);
#endif

  if (first)
  fprintf(out, "0x%llx", (unsigned long long)flags);

#undef PRINT_FLAG
}

static const char *i915_ioctl_name(unsigned long request) {
  /* i915-specific ioctls */
  switch (request) {
  case DRM_IOCTL_I915_GEM_EXECBUFFER:
    return "GEM_EXECBUFFER";
  case DRM_IOCTL_I915_GEM_EXECBUFFER2:
    return "GEM_EXECBUFFER2";
  case DRM_IOCTL_I915_GEM_EXECBUFFER2_WR:
    return "GEM_EXECBUFFER2_WR";
  case DRM_IOCTL_I915_GEM_CREATE:
    return "GEM_CREATE";
  case DRM_IOCTL_I915_GEM_CREATE_EXT:
    return "GEM_CREATE_EXT";
  case DRM_IOCTL_I915_GEM_SET_DOMAIN:
    return "GEM_SET_DOMAIN";
  case DRM_IOCTL_I915_GEM_GET_TILING:
    return "GEM_GET_TILING";
  case DRM_IOCTL_I915_GEM_SET_TILING:
    return "GEM_SET_TILING";
  case DRM_IOCTL_I915_GEM_BUSY:
    return "GEM_BUSY";
  case DRM_IOCTL_I915_GEM_MMAP:
    return "GEM_MMAP";
  case DRM_IOCTL_I915_GEM_MMAP_GTT:
    return "GEM_MMAP_GTT";
  case DRM_IOCTL_I915_GEM_MMAP_OFFSET:
    return "GEM_MMAP_OFFSET";
  case DRM_IOCTL_I915_GEM_PREAD:
    return "GEM_PREAD";
  case DRM_IOCTL_I915_GEM_PWRITE:
    return "GEM_PWRITE";
  case DRM_IOCTL_I915_GEM_THROTTLE:
    return "GEM_THROTTLE";
  case DRM_IOCTL_I915_GEM_CONTEXT_CREATE:
    return "GEM_CONTEXT_CREATE";
  case DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT:
    return "GEM_CONTEXT_CREATE_EXT";
  case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
    return "GEM_CONTEXT_DESTROY";
  case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
    return "GEM_CONTEXT_SETPARAM";
  case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM:
    return "GEM_CONTEXT_GETPARAM";
  case DRM_IOCTL_I915_GEM_USERPTR:
    return "GEM_USERPTR";
  case DRM_IOCTL_I915_GEM_WAIT:
    return "GEM_WAIT";
  case DRM_IOCTL_I915_GEM_SW_FINISH:
    return "GEM_SW_FINISH";
  case DRM_IOCTL_I915_GEM_GET_APERTURE:
    return "GEM_GET_APERTURE";
  case DRM_IOCTL_I915_GEM_SET_CACHING:
    return "GEM_SET_CACHING";
  case DRM_IOCTL_I915_GEM_GET_CACHING:
    return "GEM_GET_CACHING";
  case DRM_IOCTL_I915_REG_READ:
    return "REG_READ";
  case DRM_IOCTL_I915_GETPARAM:
    return "GETPARAM";
  case DRM_IOCTL_I915_SETPARAM:
    return "SETPARAM";
  case DRM_IOCTL_I915_GEM_MADVISE:
    return "GEM_MADVISE";
#ifdef DRM_IOCTL_I915_GEM_PIN
  case DRM_IOCTL_I915_GEM_PIN:
    return "GEM_PIN";
#endif
#ifdef DRM_IOCTL_I915_GEM_UNPIN
  case DRM_IOCTL_I915_GEM_UNPIN:
    return "GEM_UNPIN";
#endif
#ifdef DRM_IOCTL_I915_GEM_ENTERVT
  case DRM_IOCTL_I915_GEM_ENTERVT:
    return "GEM_ENTERVT";
#endif
#ifdef DRM_IOCTL_I915_GEM_LEAVEVT
  case DRM_IOCTL_I915_GEM_LEAVEVT:
    return "GEM_LEAVEVT";
#endif
#ifdef DRM_IOCTL_I915_GEM_SET_EXEC_TIMEOUT
  case DRM_IOCTL_I915_GEM_SET_EXEC_TIMEOUT:
    return "GEM_SET_EXEC_TIMEOUT";
#endif
#ifdef DRM_IOCTL_I915_GEM_GET_EXEC_TIMEOUT
  case DRM_IOCTL_I915_GEM_GET_EXEC_TIMEOUT:
    return "GEM_GET_EXEC_TIMEOUT";
#endif
#ifdef DRM_IOCTL_I915_GEM_GETPARAM
  case DRM_IOCTL_I915_GEM_GETPARAM:
    return "GEM_GETPARAM";
#endif
#ifdef DRM_IOCTL_I915_GEM_SHMEM_CREATE
  case DRM_IOCTL_I915_GEM_SHMEM_CREATE:
    return "GEM_SHMEM_CREATE";
#endif
#ifdef DRM_IOCTL_I915_GEM_CONTEXT_RESET_STATS
  case DRM_IOCTL_I915_GEM_CONTEXT_RESET_STATS:
    return "GEM_CONTEXT_RESET_STATS";
#endif
#ifdef DRM_IOCTL_I915_ALLOC
  case DRM_IOCTL_I915_ALLOC:
    return "ALLOC";
#endif
#ifdef DRM_IOCTL_I915_FREE
  case DRM_IOCTL_I915_FREE:
    return "FREE";
#endif
#ifdef DRM_IOCTL_I915_INIT
  case DRM_IOCTL_I915_INIT:
    return "INIT";
#endif
#ifdef DRM_IOCTL_I915_FLUSH
  case DRM_IOCTL_I915_FLUSH:
    return "FLUSH";
#endif
#ifdef DRM_IOCTL_I915_BATCHBUFFER
  case DRM_IOCTL_I915_BATCHBUFFER:
    return "BATCHBUFFER";
#endif
#ifdef DRM_IOCTL_I915_IRQ_EMIT
  case DRM_IOCTL_I915_IRQ_EMIT:
    return "IRQ_EMIT";
#endif
#ifdef DRM_IOCTL_I915_IRQ_WAIT
  case DRM_IOCTL_I915_IRQ_WAIT:
    return "IRQ_WAIT";
#endif
#ifdef DRM_IOCTL_I915_SWAP
  case DRM_IOCTL_I915_SWAP:
    return "SWAP";
#endif
#ifdef DRM_IOCTL_I915_CLIP
  case DRM_IOCTL_I915_CLIP:
    return "CLIP";
#endif
#ifdef DRM_IOCTL_I915_GEM_CONTEXT_RESET_STATS
  case DRM_IOCTL_I915_GEM_CONTEXT_RESET_STATS:
    return "GEM_CONTEXT_RESET_STATS";
#endif
#ifdef DRM_IOCTL_I915_GEM_VM_CREATE
  case DRM_IOCTL_I915_GEM_VM_CREATE:
    return "GEM_VM_CREATE";
#endif
#ifdef DRM_IOCTL_I915_GEM_VM_DESTROY
  case DRM_IOCTL_I915_GEM_VM_DESTROY:
    return "GEM_VM_DESTROY";
#endif
#ifdef DRM_IOCTL_I915_GEM_VM_BIND
  case DRM_IOCTL_I915_GEM_VM_BIND:
    return "GEM_VM_BIND";
#endif
#ifdef DRM_IOCTL_I915_GEM_VM_UNBIND
  case DRM_IOCTL_I915_GEM_VM_UNBIND:
    return "GEM_VM_UNBIND";
#endif
  default:
    return NULL;
  }
}

static const char *drm_ioctl_name(unsigned long request) {
  /* Generic DRM ioctls (syncobj, etc.) */
  switch (request) {
#ifdef DRM_IOCTL_SYNCOBJ_CREATE
  case DRM_IOCTL_SYNCOBJ_CREATE:
    return "SYNCOBJ_CREATE";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_DESTROY
  case DRM_IOCTL_SYNCOBJ_DESTROY:
    return "SYNCOBJ_DESTROY";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_WAIT
  case DRM_IOCTL_SYNCOBJ_WAIT:
    return "SYNCOBJ_WAIT";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD
  case DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD:
    return "SYNCOBJ_HANDLE_TO_FD";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE
  case DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE:
    return "SYNCOBJ_FD_TO_HANDLE";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_RESET
  case DRM_IOCTL_SYNCOBJ_RESET:
    return "SYNCOBJ_RESET";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_SIGNAL
  case DRM_IOCTL_SYNCOBJ_SIGNAL:
    return "SYNCOBJ_SIGNAL";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT
  case DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT:
    return "SYNCOBJ_TIMELINE_WAIT";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_QUERY
  case DRM_IOCTL_SYNCOBJ_QUERY:
    return "SYNCOBJ_QUERY";
#endif
#ifdef DRM_IOCTL_SYNCOBJ_EVENTFD
  case DRM_IOCTL_SYNCOBJ_EVENTFD:
    return "SYNCOBJ_EVENTFD";
#endif
  default:
    return NULL;
  }
}

static void print_ioctl_stats(void) {
  if (ioctl_count == 0) {
    fprintf(stderr, "IOCTL stats: no ioctls recorded in this interval\n");
    return;
  }

  /* Convert totals to microseconds for reporting */
  double total_us = (double)ioctl_total_ns / 1000.0;
  double avg_us = total_us / (double)ioctl_count;
  double max_us = (double)ioctl_max_ns / 1000.0;

  fprintf(stderr, "==== IOCTL STATS (last %lu frames) ===\n", (unsigned long)5000);
  fprintf(stderr, "Total IOCTLs: %lu\n", (unsigned long)ioctl_count);
  fprintf(stderr, "Total time: %.2f us\n", total_us);
  fprintf(stderr, "Average time: %.2f us\n", avg_us);
  fprintf(stderr, "Max time: %.2f us\n", max_us);
  fprintf(stderr, "Per-request breakdown:\n");

  fprintf(stderr, "last EXECBUFFER2 flags: 0x%llx (",
    (unsigned long long)execbuffer2_last_flags);
  print_execbuffer2_flags(stderr, execbuffer2_last_flags);
  fprintf(stderr, ")\n");

  fprintf(stderr, "last SYNCOBJ_WAIT flags: 0x%x (", syncobj_wait_last_flags);
  print_syncobj_wait_flags(stderr, syncobj_wait_last_flags);
  fprintf(stderr, ")\n");

  for (size_t i = 0; i < ioctl_stats_used; ++i) {
    ioctl_stat *s = &ioctl_stats[i];
    const char *name = i915_ioctl_name(s->request);
    if (!name)
      name = drm_ioctl_name(s->request);
    double total_req_us = (double)s->total_ns / 1000.0;
    double avg_req_us = (s->count ? (double)s->total_ns / (double)s->count / 1000.0 : 0.0);
    double max_req_us = (double)s->max_ns / 1000.0;
    if (name)
      fprintf(stderr, "  %s: count=%llu, total=%.2f us, avg=%.2f us, max=%.2f us, last_fd=%llu\n",
          name,
          (unsigned long long)s->count,
          total_req_us,
          avg_req_us,
          max_req_us,
          (unsigned long long)s->last_fd);
    else
      fprintf(stderr, "  0x%lx: count=%llu, total=%.2f us, avg=%.2f us, max=%.2f us, last_fd=%llu\n",
          (unsigned long)s->request,
          (unsigned long long)s->count,
          total_req_us,
          avg_req_us,
          max_req_us,
          (unsigned long long)s->last_fd);
  }

    fprintf(stderr,  "====================================\n");
}

static ioctl_stat *get_ioctl_stat(unsigned long request) {
  for (size_t i = 0; i < ioctl_stats_used; ++i) {
    if (ioctl_stats[i].request == request)
      return &ioctl_stats[i];
  }

  if (ioctl_stats_used >= IOCTL_STATS_MAX)
    return NULL;

  ioctl_stat *slot = &ioctl_stats[ioctl_stats_used++];
  memset(slot, 0, sizeof(*slot));
  slot->request = request;
  return slot;
}

int cur = 0;
xcb_window_t win;
xcb_connection_t *conn;
xcb_sync_fence_t prev_present_fence = XCB_NONE;

static void update_window_size_from_drawable(Display *dpy,
                                             GLXDrawable drawable) {
  XWindowAttributes attrs;
  if (!dpy || !drawable)
    return;

  if (!XGetWindowAttributes(dpy, (Window)drawable, &attrs))
    return;

  if (attrs.width <= 0 || attrs.height <= 0)
    return;

  win_width = attrs.width;
  win_height = attrs.height;
  printf("Using drawable size: %dx%d\n", win_width, win_height);

  if (conn && win != XCB_NONE) {
    uint32_t values[2] = {(uint32_t)win_width, (uint32_t)win_height};
    xcb_configure_window(conn, win,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         values);
    xcb_flush(conn);
  }
}

void __attribute__((constructor)) sharedgl_entry(void) {
  printf("Bypass EGL shared library loaded!\n");

  const char *ioctl_log = getenv("EGL_IOCTL_LOG");
  if (ioctl_log && strcmp(ioctl_log, "1") == 0)
    ioctl_logging_enabled = true;
  const char *syscall_log = getenv("EGL_SYSCALL_LOG");
  if (syscall_log && strcmp(syscall_log, "1") == 0)
    set_syscall_logging(1);

  /* Check and use different GPUs */
  const char *drm_node = "/dev/dri/renderD128";
  const char *egl_discrete = getenv("EGL_DISCRETE");
  if (egl_discrete && strcmp(egl_discrete, "1") == 0) {
    fprintf(stderr, "EGL_DISCRETE set; using discrete GPU DRM render node\n");
    drm_node = "/dev/dri/renderD129";
  } else {
    fprintf(stderr, "EGL_DISCRETE not set; using Integrated GPU DRM render node.\n");
  }

  glClipControl_ptr = (PFNGLCLIPCONTROLPROC)eglGetProcAddress("glClipControl");
  if (!glClipControl_ptr) {
    fprintf(stderr,
            "[hook] glClipControl not available; continuing without it\n");
  }
  printf("Using DRM node: %s\n", drm_node);

  /* Enable logging */
  // set_syscall_logging(1);

  /* Open DRM render node and create GBM device */
  int drm_fd = open(drm_node, O_RDWR | O_CLOEXEC);
  if (drm_fd < 0) {
    perror("open(drm)");
    return;
  }

  gbm = gbm_create_device(drm_fd);
  if (!gbm) {
    fprintf(stderr, "gbm_create_device failed\n");
    close(drm_fd);
    return;
  }
  printf("GBM device created\n");

  conn = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(conn)) {
    fprintf(stderr, "xcb_connect failed\n");
    return;
  }

  xcb_screen_t *screen =
      (xcb_screen_t *)xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  win = xcb_generate_id(conn);
  uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  uint32_t values[2] = {screen->black_pixel, XCB_EVENT_MASK_EXPOSURE};
  xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root, 0, 0,
                    win_width, win_height, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    screen->root_visual, mask, values);

  /* Set window title */
  const char *title = "XCB Demo Window";
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, XCB_ATOM_WM_NAME,
                      XCB_ATOM_STRING, 8, strlen(title), title);
  xcb_map_window(conn, win);
  xcb_flush(conn);

  // ask for present complete events (optional)
  xcb_present_select_input(conn, win, XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY,
                           0);
  cur = 0;
  prev_present_fence = XCB_NONE;
  printf("XCB window created\n");
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
  glFlush();
  /* Reduce flush frequency to avoid a syscall per-frame; flush once every
   * few frames so X requests can be batched. */
  static int flush_counter = 0;

  // GLsync glf = glFenceSync_ptr(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  // // glFlush();
  // /* Wait up to 1 second — you can tune/remove this for async path if you
  // export GPU sync FD */
  // // GLenum val = glClientWaitSync_ptr(glf, GL_SYNC_FLUSH_COMMANDS_BIT,
  // 1000000);
  // // printf("Value for enum: %d\n", val);
  // glDeleteSync_ptr(glf);
  // // printf("++++++++++++++++++++\n");

  /* signal the xshmfence so X can see the buffer is ready */
  xshmfence_trigger(bufs[cur].shm_fence);

  /* tell X to trigger its sync fence and present the pixmap */
  // xcb_sync_trigger_fence(conn, bufs[cur].sync_fence);

  // xcb_flush(conn);
  /* Do not chain presents with the previous present's idle_fence — that
   * serializes rendering by making each present wait for the previous one.
   * Let the X server/compositor manage ordering; keep per-buf wait_fence so
   * X waits for this buffer when needed. */
  xcb_present_pixmap(conn, win, bufs[cur].pixmap,
                     0,                    // serial
                     XCB_NONE,             // valid
                     XCB_NONE,             // update
                     0, 0,                 // x, y
                     XCB_NONE,             // target_crtc
                     bufs[cur].sync_fence, // wait_fence
                     XCB_NONE,             // idle_fence (do not chain)
                     0,                    // options
                     0, 0, 0,              // target_msc, divisor, remainder
                     0,                    // notifies_len
                     NULL);                // notifies

  /* Flush less frequently to reduce syscall/socket overhead. */
  if (++flush_counter >= 6) {
    xcb_flush(conn);
    flush_counter = 0;
  }
  cur = (cur + 1) % NUM_BUFFERS;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  frame_count++;
  if (ioctl_logging_enabled && frame_count % 5000 == 0) {
    if (ioctl_count > 0) {
      /* Print IOCTL statistics collected since last report and reset counters */
      print_ioctl_stats();

      /* Reset cumulative and per-request stats */
      ioctl_count = 0;
      ioctl_total_ns = 0;
      ioctl_max_ns = 0;
      ioctl_stats_used = 0;
      execbuffer2_last_flags = 0;
      syncobj_wait_last_flags = 0;
      memset(ioctl_stats, 0, sizeof(ioctl_stats));
    }

    //   printf("[ioctl]   last EXECBUFFER2 flags: 0x%llx (",
    //     (unsigned long long)execbuffer2_last_flags);
    //   print_execbuffer2_flags(execbuffer2_last_flags);

    //   uint64_t avg_ns = ioctl_total_ns / ioctl_count;
    //   printf("[ioctl] last 5000 frames: count=%" PRIu64
    //          " avg=%" PRIu64 " ns max=%" PRIu64 " ns\n",
    //          ioctl_count, avg_ns, ioctl_max_ns);

    //   for (size_t i = 0; i < ioctl_stats_used; ++i) {
    //     ioctl_stat *stat = &ioctl_stats[i];
    //     if (stat->count == 0)
    //       continue;
    //     uint64_t per_avg = stat->total_ns / stat->count;
    //   const char *name = i915_ioctl_name(stat->request);
    //   printf("[ioctl]   req=0x%lx (%s) count=%" PRIu64 " avg=%" PRIu64
    //  " ns max=%" PRIu64 " ns\n",
    //  stat->request, name ? name : "UNKNOWN", stat->count, per_avg,
    //  stat->max_ns);
    //   }
    // } else {
    //   printf("[ioctl] last 5000 frames: no ioctl calls observed\n");
    // }
    // ioctl_count = 0;
    // ioctl_total_ns = 0;
    // ioctl_max_ns = 0;
    // execbuffer2_last_flags = 0;
    // for (size_t i = 0; i < ioctl_stats_used; ++i) {
    //   ioctl_stats[i].count = 0;
    //   ioctl_stats[i].total_ns = 0;
    //   ioctl_stats[i].max_ns = 0;
    // }
  }
}

void glXSwapIntervalEXT(Display *d, GLXDrawable draw, int interval) {
  eglSwapInterval(eglDpy, interval);
}

static Bool (*glXMakeCurrent_ptr)(Display *dpy, GLXDrawable drawable,
                                  GLXContext ctx1) = NULL;
Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx1) {
  if (!glXMakeCurrent_ptr) {
    glXMakeCurrent_ptr = dlsym(RTLD_NEXT, "glXMakeCurrent");
    if (!glXMakeCurrent_ptr) {
      fprintf(stderr, "[hook] Failed to resolve real glXMakeCurrent()\n");
      return False;
    }
  }

  if (!bufs[0].bo)
    update_window_size_from_drawable(dpy, drawable);

  setup_egl();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  /* Disable logging */
  set_syscall_logging(0);

  return true;
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
  static GLuint current_fbo = 0;
  // Lazy resolve the real symbol once
  if (!glBindFramebuffer_ptr) {
    glBindFramebuffer_ptr = dlsym(RTLD_NEXT, "glBindFramebuffer");
    if (!glBindFramebuffer_ptr) {
      fprintf(stderr, "[hook] Failed to resolve real glBindFramebuffer()\n");
      return;
    }
  }
  // --- Your custom behavior here ---
  // fprintf(stderr, "[hook] glBindFramebuffer(target=0x%x, framebuffer=%u)\n",
  //         target, framebuffer);

  // Forward to the real function

  if (framebuffer == 0) {
    /* App is binding the default framebuffer, redirect it to our per-buffer FBO
     */
    if (current_fbo != bufs[cur].fbo) {
      glBindFramebuffer_ptr(target, bufs[cur].fbo);
      current_fbo = bufs[cur].fbo;
    }
    if (glClipControl_ptr)
      glClipControl_ptr(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
    return;
  }

  if (current_fbo != framebuffer) {
    glBindFramebuffer_ptr(target, framebuffer);
    current_fbo = framebuffer;
  }
  if (glClipControl_ptr)
    glClipControl_ptr(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
}

void (*glXGetProcAddressARB(const GLubyte *s))(void) {
  void *addr;
  static void *my_handle;
  const char *name = (const char *)s;

  /* to-do: use stripped str? */
  if (strstr(name, "glX") || strstr(name, "glBindFramebuffer") ||
      strstr(name, "glXSwapIntervalEXT")) {
    addr = dlsym(NULL, name);
    return addr;
  }

  if (!my_handle) {
    fprintf(stderr, "Opening libGL.so.1 from /opt/mesa\n");
    my_handle = dlopen("/opt/mesa/lib/x86_64-linux-gnu/libGL.so.1",
                       RTLD_NOW | RTLD_GLOBAL);
  }
  addr = real_dlsym(my_handle, name);
  return addr;
}

void (*glXGetProcAddress(const GLubyte *s))(void) {
  // printf("heyyyy @ glXGetProcAddress\n");
  return glXGetProcAddressARB(s);
}

void *dlsym(void *handle, const char *symbol) {
  if (!real_dlsym) {
    real_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
    if (!real_dlsym) {
      fprintf(stderr, "[hook] Failed to resolve real real_dlsym()\n");
      return False;
    }
  }

  void *sym = real_dlsym(handle, symbol);
  if (strstr(symbol, "SwapBuffers")) {
    // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
    // TODO: return your own replacement if needed
    return glXSwapBuffers;
  }
  if (strstr(symbol, "MakeCurrent")) {
    // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
    // TODO: return your own replacement if needed
    return glXMakeCurrent;
  }

  if (strstr(symbol, "glXGetProcAddress")) {
    // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
    // TODO: return your own replacement if needed
    return glXGetProcAddress;
  }

  if (strstr(symbol, "glXSwapIntervalEXT")) {
    // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
    // TODO: return your own replacement if needed
    return glXSwapIntervalEXT;
  }

  if (strstr(symbol, "glXChooseVisual")) {
    // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
    // TODO: return your own replacement if needed
    return glXChooseVisual;
  }

  if (strstr(symbol, "glXCreateContext")) {
    // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
    // TODO: return your own replacement if needed
    return glXCreateContext;
  }

  if (strstr(symbol, "glBindFramebuffer")) {
    // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
    // TODO: return your own replacement if needed
    return glBindFramebuffer;
  }
  return sym;
}

/* --- Syscall wrappers (log when syscall_logging_enabled) --- */

int open(const char *pathname, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
    int ret = syscall(SYS_openat, AT_FDCWD, pathname, flags, mode);
    if (syscall_logging_enabled)
      fprintf(stderr, "[syscall] open(\"%s\", 0x%x, 0%o) = %d\n", pathname,
              flags, mode, ret);
    return ret;
  } else {
    int ret = syscall(SYS_openat, AT_FDCWD, pathname, flags);
    if (syscall_logging_enabled)
      fprintf(stderr, "[syscall] open(\"%s\", 0x%x) = %d\n", pathname,
              flags, ret);
    return ret;
  }
}

int close(int fd) {
  int ret = syscall(SYS_close, fd);
  if (syscall_logging_enabled)
    fprintf(stderr, "[syscall] close(%d) = %d\n", fd, ret);
  return ret;
}

int dup(int oldfd) {
  int ret = syscall(SYS_dup, oldfd);
  if (syscall_logging_enabled)
    fprintf(stderr, "[syscall] dup(%d) = %d\n", oldfd, ret);
  return ret;
}

int fcntl(int fd, int cmd, ...) {
  va_list ap;
  va_start(ap, cmd);
  unsigned long arg = va_arg(ap, unsigned long);
  va_end(ap);
  long ret = syscall(SYS_fcntl, fd, cmd, arg);
  if (syscall_logging_enabled)
    fprintf(stderr, "[syscall] fcntl(%d, %d, 0x%lx) = %ld\n", fd, cmd,
            arg, ret);
  return (int)ret;
}

ssize_t getdents64(int fd, void *dirp, size_t count) {
  ssize_t ret = syscall(SYS_getdents64, fd, dirp, count);
  if (syscall_logging_enabled)
    fprintf(stderr, "[syscall] getdents64(%d, %p, %zu) = %zd\n", fd, dirp,
            count, ret);
  return ret;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
  ssize_t ret = syscall(SYS_readlink, path, buf, bufsiz);
  if (syscall_logging_enabled)
    fprintf(stderr, "[syscall] readlink(\"%s\", %p, %zu) = %zd\n", path,
            (void *)buf, bufsiz, ret);
  return ret;
}

int fstat(int fd, struct stat *statbuf) {
  long ret = syscall(SYS_fstat, fd, statbuf);
  if (syscall_logging_enabled)
    fprintf(stderr, "[syscall] fstat(%d, %p) = %ld\n", fd, (void *)statbuf,
            ret);
  return (int)ret;
}

int fstatat(int fd, const char* file, struct stat *statbuf, int flags) {
  int ret = (int)syscall(SYS_newfstatat, fd, file, statbuf, flags);
  if (syscall_logging_enabled)
    fprintf(stderr, "[syscall] fstatat(%d, %s, %d) = %d\n", fd,
            (file ? file : "(NULL)"), flags, ret);
  return ret;
}

/* Some binaries reference newfstat; provide a thin wrapper to intercept it */
int newfstat(int fd, struct stat *statbuf) {
  int ret = (int)syscall(SYS_fstat, fd, statbuf);
  if (syscall_logging_enabled)
    fprintf(stderr, "[syscall] newfstat(%d, %p) = %d\n", fd,
            (void *)statbuf, ret);
  return ret;
}


int ioctl(int fd, unsigned long request, ...) {
  static int (*real_ioctl)(int, unsigned long, ...) = NULL;
  if (!real_ioctl) {
    real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    if (!real_ioctl) {
      fprintf(stderr, "[hook] Failed to resolve real ioctl()\n");
      errno = ENOSYS;
      return -1;
    }
  }

  void *arg = NULL;
  va_list ap;
  va_start(ap, request);
  arg = va_arg(ap, void *);
  va_end(ap);

  if (!ioctl_logging_enabled && !syscall_logging_enabled)
    return real_ioctl(fd, request, arg);

  struct timespec start_ts;
  struct timespec end_ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
  int ret = real_ioctl(fd, request, arg);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts);

  uint64_t start_ns = (uint64_t)start_ts.tv_sec * 1000000000ULL +
                      (uint64_t)start_ts.tv_nsec;
  uint64_t end_ns = (uint64_t)end_ts.tv_sec * 1000000000ULL +
                    (uint64_t)end_ts.tv_nsec;
  uint64_t delta_ns = end_ns - start_ns;

  ioctl_count++;
  ioctl_total_ns += delta_ns;
  if (delta_ns > ioctl_max_ns)
    ioctl_max_ns = delta_ns;

  ioctl_stat *stat = get_ioctl_stat(request);
  if (stat) {
    stat->count++;
    stat->total_ns += delta_ns;
    if (delta_ns > stat->max_ns)
      stat->max_ns = delta_ns;
  }

  if (request == DRM_IOCTL_I915_GEM_EXECBUFFER2 && arg) {
    const struct drm_i915_gem_execbuffer2 *execbuf =
        (const struct drm_i915_gem_execbuffer2 *)arg;
    execbuffer2_last_flags = execbuf->flags;
  }

  if (request == DRM_IOCTL_SYNCOBJ_WAIT && arg) {
    const struct drm_syncobj_wait *wait = (const struct drm_syncobj_wait *)arg;
    syncobj_wait_last_flags = wait->flags;
  }

  if (syscall_logging_enabled) {
    fprintf(stderr, "[syscall] ioctl(fd=%d, req=0x%lx) = %d\n", fd, request,
            ret);
  }

  return ret;
}