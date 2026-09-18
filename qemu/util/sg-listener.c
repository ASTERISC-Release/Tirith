#include "qemu/osdep.h"
#include "include/qemu/sg.h"
#include "hw/core/cpu.h"
#include <linux/kvm.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/xshmfence.h>
#include <assert.h>
#include <drm/drm.h>
#include <drm/i915_drm.h>
#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/sync.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>

/* SYSCALL logging */
int syscall_logging_enabled = 0;
static uint64_t fs_offset = 500;

/* Idle sleep configuration: sleep instead of spinning when no requests arrive
 */
#define ENABLE_IDLE_SLEEP 0
#define IDLE_TIMEOUT_MS 1000 /* ms before thread sleeps */
#define IDLE_SLEEP_US 1000  /* us to sleep when idle */

/* IOCTL logging */
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

#ifdef STAT_DEBUG
static void print_syncobj_wait_flags(FILE *out, uint32_t flags) {
  if (flags == 0) {
    fprintf(out, "NONE\n");
    return;
  }

  bool first = true;
#ifdef STAT_DEBUG
#define PRINT_SYNCOBJ_FLAG(flag)                                               \
  do {                                                                         \
    if (flags & (flag)) {                                                      \
      fprintf(out, "%s%s", first ? "" : "|", #flag);                           \
      first = false;                                                           \
    }                                                                          \
  } while (0)
#else
#define PRINT_SYNCOBJ_FLAG(flag)                                               \
  do {                                                                         \
  } while (0)
#endif

  PRINT_SYNCOBJ_FLAG(DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL);
  PRINT_SYNCOBJ_FLAG(DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT);

  if (first)
    fprintf(out, "0x%x", flags);

  fprintf(out, "\n");

#undef PRINT_SYNCOBJ_FLAG
}
#else
static void print_syncobj_wait_flags(FILE *out, uint32_t flags) {}
#endif

#ifdef STAT_DEBUG
static void print_execbuffer2_flags(uint64_t flags) {
  if (flags == 0) {
    fprintf(stderr, "NONE");
    return;
  }

  bool first = true;

#ifdef STAT_DEBUG
#define PRINT_FLAG(flag)                                                       \
  do {                                                                         \
    if (flags & (flag)) {                                                      \
      fprintf(stderr, "%s%s", first ? "" : "|", #flag);                        \
      first = false;                                                           \
    }                                                                          \
  } while (0)
#else
#define PRINT_FLAG(flag)                                                       \
  do {                                                                         \
  } while (0)
#endif

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
    fprintf(stderr, "0x%llx", (unsigned long long)flags);

  fprintf(stderr, "\n");

#undef PRINT_FLAG
}
#else
static void print_execbuffer2_flags(uint64_t flags) {}
#endif

static const char *i915_ioctl_name(unsigned long request) {
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

static void print_ioctl_stats(void) {
  if (ioctl_count == 0) {
    log_stat("IOCTL stats: no ioctls recorded in this interval\n");
    return;
  }

  /* Convert totals to microseconds for reporting */
  double total_us = (double)ioctl_total_ns / 1000.0;
  double avg_us = total_us / (double)ioctl_count;
  double max_us = (double)ioctl_max_ns / 1000.0;

  log_stat("==== IOCTL STATS (last %lu frames) ===\n", (unsigned long)5000);
  log_stat("Total IOCTLs: %lu\n", (unsigned long)ioctl_count);
  log_stat("Total time: %.2f us\n", total_us);
  log_stat("Average time: %.2f us\n", avg_us);
  log_stat("Max time: %.2f us\n", max_us);
  log_stat("Per-request breakdown:\n");

  log_stat("last EXECBUFFER2 flags: 0x%llx => ",
           (unsigned long long)execbuffer2_last_flags);
  print_execbuffer2_flags(execbuffer2_last_flags);

  log_stat("last SYNCOBJ_WAIT flags: 0x%x => ", syncobj_wait_last_flags);
  print_syncobj_wait_flags(stderr, syncobj_wait_last_flags);

  for (size_t i = 0; i < ioctl_stats_used; ++i) {
    ioctl_stat *s = &ioctl_stats[i];
    const char *name = i915_ioctl_name(s->request);
    double total_req_us = (double)s->total_ns / 1000.0;
    double avg_req_us =
        (s->count ? (double)s->total_ns / (double)s->count / 1000.0 : 0.0);
    double max_req_us = (double)s->max_ns / 1000.0;
    if (name)
      log_stat("  %s: count=%llu, total=%.2f us, avg=%.2f us, max=%.2f us, "
               "last_fd=%llu\n",
               name, (unsigned long long)s->count, total_req_us, avg_req_us,
               max_req_us, (unsigned long long)s->last_fd);
    else
      log_stat("  0x%lx: count=%llu, total=%.2f us, avg=%.2f us, max=%.2f us, "
               "last_fd=%llu\n",
               (unsigned long)s->request, (unsigned long long)s->count,
               total_req_us, avg_req_us, max_req_us,
               (unsigned long long)s->last_fd);
  }

  log_stat("====================================\n");
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

struct timespec ts;
void *data_region_actual_address = NULL;
void *hugepage_host_addr = NULL;
typedef struct {
  uint64_t host_address;
  uint64_t guest_address;
} gem_slots_t;
gem_slots_t gem_slots = {0};
gem_slots_t gem_hugepage_slots = {0};

xcb_window_t win;
xcb_connection_t *conn;

static long mmap_freq = 0;
static long ioctl_freq = 0;
// static long frame_count = 0;
static double frame_latency = 0.0;
static double time_spent_in_ioctl = 0.0;
static double start_frame = 0.0;
static uint64_t execbuf_count = 0;

static volatile bool qemu_setup_done = false;
static check *bufs_persistent = NULL;
static pthread_mutex_t gem_slots_lock = PTHREAD_MUTEX_INITIALIZER;

static inline uint64_t gem_regular_advance(uint64_t size) {
  return PAGE_SIZE * (int)((PAGE_SIZE + size) / PAGE_SIZE);
}

static inline bool gem_bucket_has_capacity(const gem_slots_t *slots,
                                           uint64_t size,
                                           uint64_t region_base,
                                           uint64_t region_size) {
  return slots->host_address + size < region_base + region_size;
}

/* Defined in egl-redirect/xcb.c */
void create_pixmap_from_kbuf(check *bufs, int i, uint32_t size_bytes,
                             uint32_t stride);
int create_xcb_fence(check *bufs, int buf_index);
void create_and_setup_xcb_window(void);
void update_window(void);
void xcb_present(check *bufs, int cur);
int query_idle_fence_buffer(check *bufs, int cur);

int win_width = 300;
int win_height = 300;
uint64_t handle_to_size[1024];

int get_guest_tsc_info(uint64_t *offset, uint32_t *frequency_khz) {
  if (!first_cpu || !offset || !frequency_khz)
    return -EINVAL;

  struct kvm_device_attr attribute = {
      .group = KVM_VCPU_TSC_CTRL,
      .attr = KVM_VCPU_TSC_OFFSET,
      .addr = (uint64_t)(uintptr_t)offset,
  };
  int ret = ioctl(first_cpu->kvm_fd, KVM_GET_DEVICE_ATTR, &attribute);
  if (ret < 0)
    return -errno;

  ret = ioctl(first_cpu->kvm_fd, KVM_GET_TSC_KHZ, 0);
  if (ret < 0)
    return -errno;

  *frequency_khz = (uint32_t)ret;
  return 0;
}

static inline void complete_request(volatile comm_page_t *c) {
  __atomic_store_n(&c->req_bit, 0, __ATOMIC_RELEASE);
}

void setup_comm_data_regions(volatile comm_page_t *c) {
  while (c->magic != COMM_MAGIC) {
    usleep(1000);
  }
  log_always("COMM: 0x%llx\n", (unsigned long long)(uint64_t)(uintptr_t)c);
  log_always("COMM MAGIC: %p\n", (void *)*((uint64_t *)COMM_ADDR));
  c->magic = 0x2;

  volatile comm_page_t *d = (comm_page_t *)(uintptr_t)DATA_REGION;
  while (d->magic != 0x1234567812344678ULL) {
    usleep(1000);
  }
  data_region_actual_address = (void *)((uint64_t)global_ram_address);

  if (data_region_actual_address != DATA_REGION) {
    log_always("FATAL: data region not identity-mapped (guest=%p host=%p)\n",
               (void *)(uintptr_t)DATA_REGION,
               (void *)(uintptr_t)data_region_actual_address);
    assert(false);
  }

  log_always("DATA MAGIC: %p\n", (void *)*((uint64_t *)DATA_REGION));
  log_always("DATA REGION: (guest=%p, host=%p)\n",
             (void *)(uint64_t)DATA_REGION, data_region_actual_address);

  while (*((uint64_t *)data_region_actual_address) != 0x1234567812344678ULL) {
    usleep(1000);
  }
  *((uint64_t *)data_region_actual_address) = 0x2;

  gem_slots.host_address = ((uint64_t)data_region_actual_address);
  gem_slots.guest_address = ((uint64_t)DATA_REGION);

  // Wait for hugedata addr
  while (c->p1 == 0) {
    usleep(1000);
  }
  // Set hugepage_host_addr
  hugepage_host_addr = (void *)((uint64_t)(c->p1 - (uint64_t)0x180000000ULL) +
                                (uint64_t)global_ram3_address);
  HUGEPAGE_DATA_REGION = (void *)c->p1;

  if (hugepage_host_addr != HUGEPAGE_DATA_REGION) {
    log_always(
        "FATAL: hugepage region not identity-mapped (guest=%p host=%p)\n",
        (void *)(uintptr_t)HUGEPAGE_DATA_REGION,
        (void *)(uintptr_t)hugepage_host_addr);
    assert(false);
  }

  c->p1 = 0; // clear addr arg
  log_always("HUGEPAGE_DATA REGION: (guest=%p, host=%p)\n",
             (void *)(uint64_t)HUGEPAGE_DATA_REGION, hugepage_host_addr);

  while (*((uint64_t *)hugepage_host_addr) != 0x1234567812344678ULL) {
    usleep(1000);
  }
  log_always("HUGEPAGE_DATA MAGIC: %p\n",
             (void *)*((uint64_t *)HUGEPAGE_DATA_REGION));
  *((uint64_t *)hugepage_host_addr) = 0x2;

  gem_hugepage_slots.host_address = ((uint64_t)hugepage_host_addr);
  gem_hugepage_slots.guest_address = ((uint64_t)HUGEPAGE_DATA_REGION);

  create_and_setup_xcb_window();
  log_always("XCB window created\n");

  c->ret = 0;
  __atomic_store_n(&c->magic, 0x3, __ATOMIC_RELEASE);
}

extern void *mmap_listener(void *arg) {
  long index = (long)arg;

  /* TODO: Is this really needed? */
  // cpu_set_t cpuset;
  // CPU_ZERO(&cpuset);
  // CPU_SET(3 + index, &cpuset);
  // pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  /* Setup the communications and data regions */
  volatile comm_page_t *c =
      (comm_page_t *)(uintptr_t)(COMM_ADDR + (index * sizeof(comm_page_t)));

  if (index == 0) {
    setup_comm_data_regions(c);
    qemu_setup_done = true;
  } else {
    while (!qemu_setup_done) {
      usleep(1000);
    }
    while (c->magic != COMM_MAGIC) {
      usleep(1000);
    }
    c->magic = 0x2;
  }

  void *curr_host_addr = NULL;
  void *curr_guest_addr = NULL;

  /*
   * Event Processing loop
   */
  uint64_t ret;

#if ENABLE_IDLE_SLEEP
  struct timespec last_request_time;
  clock_gettime(CLOCK_MONOTONIC, &last_request_time);
#endif

  for (;;) {
    RequestType req;

#if ENABLE_IDLE_SLEEP
    req = (RequestType)__atomic_load_n(&c->req_bit, __ATOMIC_ACQUIRE);
    if (req == 0) {
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      long elapsed_ms = (now.tv_sec - last_request_time.tv_sec) * 1000 +
                        (now.tv_nsec - last_request_time.tv_nsec) / 1000000;
      if (elapsed_ms >= IDLE_TIMEOUT_MS) {
        usleep(IDLE_SLEEP_US);
      }
      continue;
    }
    clock_gettime(CLOCK_MONOTONIC, &last_request_time);
#else
    /* This is a latency-sensitive SPSC doorbell. Keep the idle path to one load and one branch;
     * running the full switch range check for every empty poll needlessly halves the listener's
     * polling rate. The acquire load pairs with the guest's release publication and makes the
     * request payload visible before dispatch. */
    do {
      req = (RequestType)__atomic_load_n(&c->req_bit, __ATOMIC_ACQUIRE);
    } while (__builtin_expect(req == 0, true));
#endif

    switch (req) {
    case SYSCALL_LOGGING_ENABLE:
      set_syscall_logging(1);
      log_sg("Syscall logging enabled\n");
      complete_request(c);
      break;

    case SYSCALL_LOGGING_DISABLE:
      set_syscall_logging(0);
      log_sg("Syscall logging disabled\n");
      complete_request(c);
      break;

    case GEM_ALLOCATION:
      uint64_t size = c->p1;
      bool use_hugepage_bucket = (size % 0x200000 == 0);
      bool hugepage_has_capacity =
          gem_bucket_has_capacity(&gem_hugepage_slots, size,
                                  (uint64_t)(uintptr_t)hugepage_host_addr,
                                  HUGE_DATA_SIZE);
      bool regular_has_capacity =
          gem_bucket_has_capacity(&gem_slots, size,
                                  (uint64_t)(uintptr_t)data_region_actual_address,
                                  DATA_SIZE);

      if (use_hugepage_bucket && !hugepage_has_capacity && regular_has_capacity) {
        log_gem("hugepage bucket full for size 0x%lx, falling back to regular bucket\n",
                size);
        use_hugepage_bucket = false;
      } else if (!use_hugepage_bucket && !regular_has_capacity &&
                 hugepage_has_capacity) {
        log_gem("regular bucket full for size 0x%lx, falling back to hugepage bucket\n",
                size);
        use_hugepage_bucket = true;
      }

      if (use_hugepage_bucket) {
        /* Hugepage capable GEM allocation */
        log_gem("allocation: HUGEPAGE\n");
        log_gem("size: 0x%lx, host: 0x%lx, guest: 0x%lx (fd = %d)\n", size,
                gem_hugepage_slots.host_address,
                gem_hugepage_slots.guest_address, c->p4);

        if (gem_hugepage_slots.host_address !=
            gem_hugepage_slots.guest_address) {
          log_always("FATAL: GEM hugepage slots not identity-mapped "
                     "(host=0x%lx guest=0x%lx size=0x%lx)\n",
                     gem_hugepage_slots.host_address,
                     gem_hugepage_slots.guest_address, size);
          assert(false);
        }

        // Unmapping previous mapping (and asserts)
        assert(hugepage_has_capacity);
        assert(munmap((void *)(uintptr_t)gem_hugepage_slots.host_address,
                      size) == 0);

        // Mapping on original offset
        void *retptr = mmap(
            (void *)(uintptr_t)gem_hugepage_slots.host_address, c->p1 /*size*/,
            c->p2, c->p3 | MAP_SHARED | MAP_FIXED, c->p4 - fs_offset, c->p5);
        if (retptr == MAP_FAILED) {
          perror("[QEMU-HOST] MMAP failed for GEM_ALLOCATION!!!!!");
          assert(false);
        }
        assert(retptr == (void *)(uintptr_t)gem_hugepage_slots.host_address);

        c->ret = (uint64_t)gem_hugepage_slots.host_address;
        pthread_mutex_lock(&gem_slots_lock);
        gem_hugepage_slots.host_address += size;
        gem_hugepage_slots.guest_address += size;
        pthread_mutex_unlock(&gem_slots_lock);

      } else {
        /* Regular pages for GEM allocation */
        log_gem("size: 0x%lx, host: 0x%lx, guest: 0x%lx\n", size,
                gem_slots.host_address, gem_slots.guest_address);

        if (gem_slots.host_address != gem_slots.guest_address) {
          log_always("FATAL: GEM slots not identity-mapped (host=0x%lx "
                     "guest=0x%lx size=0x%lx)\n",
                     gem_slots.host_address, gem_slots.guest_address, size);
          assert(false);
        }

        // Unmapping previous mapping (and asserts)
        assert(regular_has_capacity);
        assert(munmap((void *)(uintptr_t)gem_slots.host_address, size) == 0);

        // Mapping on original offset
        void *retptr = mmap(
            (void *)(uintptr_t)gem_slots.host_address, c->p1 /*size*/, c->p2,
            c->p3 | MAP_SHARED | MAP_FIXED, c->p4 - fs_offset, c->p5);
        if (retptr == MAP_FAILED) {
          perror("[QEMU-HOST] MMAP failed for GEM_ALLOCATION!!!!!");
          assert(false);
        }
        assert(retptr == (void *)(uintptr_t)gem_slots.host_address);

        c->ret = (uint64_t)gem_slots.host_address;
        pthread_mutex_lock(&gem_slots_lock);
        gem_slots.host_address += gem_regular_advance(size);
        gem_slots.guest_address += gem_regular_advance(size);
        pthread_mutex_unlock(&gem_slots_lock);
      }

      complete_request(c);

      log_sg("mmap() returned: 0x%lx\n", c->ret);
      mmap_freq++;
      break;

    case FSTAT:
      ret = fstat(c->p1 - fs_offset, (struct stat *)c->p2);
      c->ret = ret;
      log_sg("[syscall] fstat(%d, %p) = %ld\n", c->p1, (void *)c->p2, ret);
      complete_request(c);
      break;

    case IOCTL: {
      // uint64_t start,end;
      // struct timespec start_ts;
      // struct timespec end_ts;
      // uint64_t req_type = _IOC_NR(c->p2);
      // int already_done = 0;

      // /* Log the time it takes for the IOCTLs */
      // clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
      ret = ioctl(c->p1 - fs_offset, c->p2, (void *)c->p3);
      // clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts);

      // uint64_t start_ns = (uint64_t)start_ts.tv_sec * 1000000000ULL +
      //                     (uint64_t)start_ts.tv_nsec;
      // uint64_t end_ns = (uint64_t)end_ts.tv_sec * 1000000000ULL +
      //                     (uint64_t)end_ts.tv_nsec;
      // uint64_t delta_ns = end_ns - start_ns;

      // ioctl_count++;
      // ioctl_total_ns += delta_ns;
      // if (delta_ns > ioctl_max_ns)
      //     ioctl_max_ns = delta_ns;

      /* track additional derived counters */
      // ioctl_freq++;
      /* keep time_spent_in_ioctl in microseconds for consistency */
      // time_spent_in_ioctl += (double)delta_ns / 1000.0; /* microseconds */

      //       ioctl_stat *stat = get_ioctl_stat(c->p2);
      //       if (stat) {
      //         stat->count++;
      //         stat->total_ns += delta_ns;
      //         if (delta_ns > stat->max_ns)
      //           stat->max_ns = delta_ns;
      //         stat->last_fd = c->p1;
      //       }

      // if (c->p2 == DRM_IOCTL_I915_GEM_CREATE_EXT) {
      //     struct drm_i915_gem_create_ext *create = (struct
      //     drm_i915_gem_create_ext *)c->p3;
      //     // We need the handle AFTER the ioctl returns
      //     if (ret == 0 && create->handle < 1024) {
      //         handle_to_size[create->handle] = create->size;
      //         log_sg("Tracking BO: Handle %u, Size %llu\n", create->handle,
      //         create->size);
      //     }
      // }
      // if (c->p2 == DRM_IOCTL_I915_GEM_EXECBUFFER2 && c->p3) {
      //     const struct drm_i915_gem_execbuffer2 *execbuf =
      //         (const struct drm_i915_gem_execbuffer2 *)c->p3;
      //     execbuffer2_last_flags = execbuf->flags;

      //     struct drm_i915_gem_exec_object2 *objs = (struct
      //     drm_i915_gem_exec_object2 *)(uintptr_t)execbuf->buffers_ptr;

      //     for (int i = 0; i < execbuf->buffer_count; i++) {
      //         uint32_t h = objs[i].handle;
      //         uint64_t s = (h < 1024) ? handle_to_size[h] : 0;

      //         if (s == 4194304) {
      //             log_sg("Found 4MB BO in ExecBuf! Handle: %u, GPU Offset:
      //             0x%llx\n", h, objs[i].offset);
      //             // 1. Prepare the mmap request
      //             struct drm_i915_gem_mmap mmap_arg = {0};
      //             mmap_arg.handle = h;
      //             mmap_arg.size = s;
      //             mmap_arg.offset = 0; // In this ioctl, offset is for the BO
      //             internal offset

      //             // 2. Ask the driver to map this BO into our address space
      //             if (ioctl(c->p1 - fs_offset, DRM_IOCTL_I915_GEM_MMAP,
      //             &mmap_arg) == 0) {
      //                 unsigned char* ptr = (unsigned char*)mmap_arg.addr_ptr;
      //
      //                 // 3. Dump the first 256 bytes
      //                 log_sg("--- BO Header Dump (First 256B) ---\n");
      //                 for (int j = 0; j < 256; j++) {
      //                     log_sg("%02x ", ptr[j]);
      //                     if ((j + 1) % 16 == 0) log_sg("\n");
      //                 }

      //                 // 4. Dump 256 bytes from the center (2MB mark)
      //                 log_sg("--- BO Center Dump (256B from 2MB offset)
      //                 ---\n"); unsigned char* center_ptr = ptr + (2 * 1024 *
      //                 1024); for (int j = 0; j < 256; j++) {
      //                     log_sg("%02x ", center_ptr[j]);
      //                     if ((j + 1) % 16 == 0) log_sg("\n");
      //                 }

      //                 // 5. Cleanup: Always unmap when done!
      //                 munmap(ptr, s);
      //             } else {
      //                 log_sg("Failed to mmap BO handle %u\n", h);
      //             }
      //         }
      //     }
      // }

      // if (c->p2 == DRM_IOCTL_SYNCOBJ_WAIT && c->p3) {
      //     const struct drm_syncobj_wait *wait = (const struct
      //     drm_syncobj_wait *)c->p3; syncobj_wait_last_flags = wait->flags;
      // }

      c->ret = ret;

      // log_sg("[syscall] ioctl(fd=%d, req=0x%lx) = %d\n", c->p1 - fs_offset,
      // (void*) c->p2, ret);
      complete_request(c);
      break;
    }

    case OPEN:
      int oret = open((const char *)c->p1, c->p2, c->p3);
      if (oret < 0) {
        int open_errno = errno;
        log_sg("[syscall] open(\"%s\", %#lx) failed: %s\n",
               (const char *)c->p1, c->p2, strerror(open_errno));
        c->ret = (uint64_t)(int64_t)-open_errno;
      } else {
        c->ret = oret + fs_offset;
        // fprintf(stderr, "[QEMU] open(\"%s\", 0x%x) = %d (guest fd: %d)\n",
        // (const char*) c->p1, c->p2, oret, c->ret);
      }
      log_sg("[syscall] open(\"%s\", 0x%x) = %d\n", (const char *)c->p1, c->p2,
             oret);
      complete_request(c);
      break;

    case FCNTL:
      ret = fcntl(c->p1 - fs_offset, c->p2, c->p3);
      if (c->p2 == F_DUPFD_CLOEXEC) {
        c->ret = ret + fs_offset;
      } else {
        c->ret = ret;
      }
      log_sg("[syscall] fcntl(%d, %d, 0x%lx) = %ld\n", c->p1, c->p2, c->p3,
             ret);
      complete_request(c);
      break;

    case HOST_FLOCK:
      ret = flock(c->p1 - fs_offset, c->p2);
      c->ret = ret < 0 ? -errno : ret;
      log_sg("[syscall] flock(%d, 0x%lx) = %ld\n", c->p1, c->p2, ret);
      complete_request(c);
      break;

    case HOST_FTRUNCATE:
      ret = ftruncate(c->p1 - fs_offset, c->p2);
      c->ret = ret < 0 ? -errno : ret;
      log_sg("[syscall] ftruncate(%d, 0x%lx) = %ld\n", c->p1, c->p2, ret);
      complete_request(c);
      break;

    case LSEEK:
      ret = lseek(c->p1 - fs_offset, c->p2, c->p3);
      c->ret = ret;
      log_sg("[syscall] lseek(%d, %d, 0x%lx) = %ld\n", c->p1, c->p2, c->p3,
             ret);
      complete_request(c);
      break;

    case READLINK:
      ret = readlink((const char *)c->p1, (const char *)c->p2, c->p3);
      c->ret = ret;
      log_sg("[syscall] readlink(\"%s\", %p, %zu) = %zd\n", (const char *)c->p1,
             (void *)c->p2, c->p3, ret);
      complete_request(c);
      break;

    case NEWFSTAT:
      ret = fstatat(c->p1 - fs_offset, (const char *)c->p2,
                    (struct stat *)c->p3, c->p4);
      c->ret = ret;
      complete_request(c);
      log_sg("[syscall] newfstat(%d, %p) = %d\n", c->p1, (void *)c->p3, ret);
      break;

    case GETDENT:
      ret = syscall(SYS_getdents64, c->p1 - fs_offset, c->p2, c->p3);
      c->ret = ret;
      complete_request(c);
      log_sg("[syscall] getdents64(%d, %p, %zu) = %zd\n", c->p1, c->p2, c->p3,
             ret);
      break;

    case DUP:
      ret = dup(c->p1 - fs_offset);
      c->ret = ret + fs_offset;
      log_sg("[syscall] dup(%d) = %d\n", c->p1, ret);
      complete_request(c);
      break;

    case DUP_IDENTITY:
      ret = dup(c->p1);
      c->ret = ret + fs_offset;
      log_sg("[syscall] dup(%d) = %d\n", c->p1, ret);
      complete_request(c);
      break;

    case X11_SETUP:
      create_pixmap_from_kbuf((check *)c->p1, c->p2, c->p3, c->p4);
      create_xcb_fence((check *)c->p1, c->p2);
      log_sg("Completed mapping XCB pixmap and fence for buffer index %d\n",
             c->p2);
      complete_request(c);
      break;

    case X11_PRESENT:
      check *tmp_buf = (check *)c->p1;
      int cur = c->p2;
      xcb_present(tmp_buf, cur);

      complete_request(c);
      frame_count++;

      // if(frame_count%5000 == 0){
      //     /* Print IOCTL statistics collected since last report and reset
      //     counters */ print_ioctl_stats();

      //     /* Reset cumulative and per-request stats */
      //     ioctl_count = 0;
      //     ioctl_total_ns = 0;
      //     ioctl_max_ns = 0;
      //     ioctl_stats_used = 0;
      //     execbuffer2_last_flags = 0;
      //     syncobj_wait_last_flags = 0;
      //     memset(ioctl_stats, 0, sizeof(ioctl_stats));

      //     /* Reset runtime counters */
      //     frame_latency = 0;
      //     ioctl_freq = 0;
      //     mmap_freq = 0;
      //     time_spent_in_ioctl = 0;
      //     execbuf_count = 0;
      // }
      break;

    case X11_IDLE_FENCE_BUFFER:
      tmp_buf = (check *)c->p1;
      cur = c->p2;
      c->ret = query_idle_fence_buffer(tmp_buf, cur);

      complete_request(c);
      break;

    case CLOSE:
      close(c->p1 - 500);
      log_sg("[syscall] close(%d) = %d\n", c->p1, ret);
      complete_request(c);
      break;

    case UPDATE_WINDOW_SIZE:
      log_sg("Received request to update window size to %dx%d\n", c->p1, c->p2);
      win_width = c->p1;
      win_height = c->p2;
      if (c->p3) {
        xcb_window_t app_win = (xcb_window_t)c->p3;
        set_presentation_window(app_win);
      }
      /* The application and the window manager own the real window's
       * geometry, focus and input. update_window() only resizes QEMU's
       * embedded DRI3 presentation surface. */
      update_window();
      complete_request(c);
      break;

    case UPDATE_INPUT_MASK:
      set_shared_input_mask((xcb_window_t)c->p1, (uint32_t)c->p2);
      c->ret = 0;
      complete_request(c);
      break;

    case READ:
      ret = read(c->p1 - 500, (void *)(uintptr_t)c->p2, c->p3);
      c->ret = ret;
      log_sg("[syscall] read(%d, %d, %d) = %d\n", c->p1, c->p2, c->p3, ret);
      complete_request(c);
      break;

    case WRITE:
      ret = write(c->p1 - 500, (const void *)(uintptr_t)c->p2, c->p3);
      c->ret = ret;
      log_sg("[syscall] write(%d, 0x%lx, %d) = %d\n", c->p1, c->p2, c->p3, ret);
      complete_request(c);
      break;

    case SYSV_SEMGET: {
      int semid = semget((key_t)c->p1, (int)c->p2, (int)c->p3);
      c->ret = semid < 0 ? (uint64_t)(int64_t)-errno : (uint64_t)semid;
      complete_request(c);
      break;
    }

    case SYSV_SEMCTL: {
      int semret = semctl((int)c->p1, (int)c->p2, (int)c->p3,
                          (unsigned long)c->p4);
      c->ret = semret < 0 ? (uint64_t)(int64_t)-errno : (uint64_t)semret;
      complete_request(c);
      break;
    }

    case SYSV_SEMOP: {
      struct sembuf op = {
          .sem_num = (unsigned short)c->p2,
          .sem_op = (short)c->p3,
          /* Never block the single request listener. The LibOS retries blocking
           * operations so another guest thread can still submit the matching post. */
          .sem_flg = (short)c->p4 | IPC_NOWAIT,
      };
      int semret = semop((int)c->p1, &op, 1);
      c->ret = semret < 0 ? (uint64_t)(int64_t)-errno : (uint64_t)semret;
      complete_request(c);
      break;
    }

    default:
      break;
    }
  }
  return NULL;
}
