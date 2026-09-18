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
#include <math.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/xfixes.h>

#include <X11/xshmfence.h>
#include <drm/drm_fourcc.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "bypass-egl.h"
#include <X11/xshmfence.h>

check bufs[NUM_BUFFERS];
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ptr = NULL;
PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ptr;
PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ptr;
struct gbm_device *gbm;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_ptr;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers_ptr;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer_ptr;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage_ptr;
GLuint fbo;
EGLDisplay eglDpy;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ptr;
int win_width = DEFAULT_WIDTH;
int win_height = DEFAULT_HEIGHT;

int test_prime_to_handle(int dma_fd) {
    const char *drm_node = "/dev/dri/renderD128";
    int drm_fd = open(drm_node, O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
      perror("open(drm)");
      return;
    }

    struct drm_prime_handle p = { .fd = dma_fd, .flags = 0 };
    if (ioctl(drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &p) < 0) {
        perror("PRIME_FD_TO_HANDLE");
        return -1;
    }
    fprintf(stderr, "PRIME_FD_TO_HANDLE OK, handle=%u\n", p.handle);
    return 0;
}

void setup_egl() {
  eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
      "eglGetPlatformDisplayEXT");
  eglDpy = EGL_NO_DISPLAY;

  if (eglGetPlatformDisplayEXT)
    eglDpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm, NULL);

  if (eglDpy == EGL_NO_DISPLAY) {
    eglDpy = eglGetDisplay((EGLNativeDisplayType)NULL);
    printf("No display\n");
    assert(false);
  }

  if (!eglDpy) {
    fprintf(stderr, "eglGetDisplay failed\n");
    assert(false);
  }

  if (!eglInitialize(eglDpy, NULL, NULL)) {
    check_egl_error("eglInitialize");
    assert(false);
  }

  EGLint major, minor;
  eglInitialize(eglDpy, &major, &minor);
  printf("EGL initialized: version %d.%d\n", major, minor);

  EGLint cfgAttrs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE, // don’t request pbuffer
                       EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};

  EGLConfig cfg;
  EGLint numCfg = 0;
  if (!eglChooseConfig(eglDpy, cfgAttrs, &cfg, 1, &numCfg) || numCfg == 0) {
    fprintf(stderr, "eglChooseConfig failed\n");
    assert(false);
  }

  // Bind desktop GL API
  if (!eglBindAPI(EGL_OPENGL_API)) {
    fprintf(stderr, "eglBindAPI failed\n");
    assert(false);
  }

  // Create context with no surface
  EGLContext ctx = eglCreateContext(eglDpy, cfg, EGL_NO_CONTEXT, NULL);
  if (ctx == EGL_NO_CONTEXT) {
    check_egl_error("eglCreateContext");
    assert(false);
  }
  // Make current with no surface (surfaceless)
  if (!eglMakeCurrent(eglDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    check_egl_error("eglMakeCurrent");
    assert(false);
  }

  eglCreateImageKHR_ptr =
      (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  eglDestroyImageKHR_ptr =
      (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  glEGLImageTargetTexture2DOES_ptr =
      (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
          "glEGLImageTargetTexture2DOES");

  glGenFramebuffers_ptr =
      (PFNGLGENFRAMEBUFFERSPROC)eglGetProcAddress("glGenFramebuffers");
  glBindFramebuffer_ptr =
      (PFNGLBINDFRAMEBUFFERPROC)eglGetProcAddress("glBindFramebuffer");
  glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)eglGetProcAddress(
      "glFramebufferTexture2D");
  glFramebufferRenderbuffer_ptr =
      (PFNGLFRAMEBUFFERRENDERBUFFERPROC)eglGetProcAddress(
          "glFramebufferRenderbuffer");
  glGenRenderbuffers_ptr =
      (PFNGLGENRENDERBUFFERSPROC)eglGetProcAddress("glGenRenderbuffers");
  glBindRenderbuffer_ptr =
      (PFNGLBINDRENDERBUFFERPROC)eglGetProcAddress("glBindRenderbuffer");
  glRenderbufferStorage_ptr =
      (PFNGLRENDERBUFFERSTORAGEPROC)eglGetProcAddress("glRenderbufferStorage");
  glCheckFramebufferStatus_ptr =
      (PFNGLCHECKFRAMEBUFFERSTATUSPROC)eglGetProcAddress(
          "glCheckFramebufferStatus");
  glFenceSync_ptr = (PFNGLFENCESYNCPROC)eglGetProcAddress("glFenceSync");
  glClientWaitSync_ptr =
      (PFNGLCLIENTWAITSYNCPROC)eglGetProcAddress("glClientWaitSync");
  glDeleteSync_ptr = (PFNGLDELETESYNCPROC)eglGetProcAddress("glDeleteSync");

  if (!eglCreateImageKHR_ptr || !eglDestroyImageKHR_ptr ||
      !glEGLImageTargetTexture2DOES_ptr || !glGenFramebuffers_ptr ||
      !glBindFramebuffer_ptr || !glFramebufferTexture2D_ptr ||
      !glFramebufferRenderbuffer_ptr || !glCheckFramebufferStatus_ptr ||
      !glGenRenderbuffers_ptr || !glBindRenderbuffer_ptr ||
      !glRenderbufferStorage_ptr || !glFenceSync_ptr) {
    fprintf(stderr, "Missing required EGL/GL entry points\n");
    assert(false);
  }

  memset(bufs, 0, sizeof(bufs));
  // sleep(10);
  /* Create two GBM BOs, pixmaps, EGLImages and textures */
  for (int i = 0; i < NUM_BUFFERS; ++i) {

    /*
        This is the idea:

            the following indented snippet will be executed at the HOST. and it
       will return a FD (int)

            IOCTLs made:

            [pid 111638] ioctl(5, DRM_IOCTL_I915_GEM_CREATE_EXT, 0x7fff1f9e9c08)
       = 0 [pid 111638] ioctl(5, DRM_IOCTL_I915_GEM_SET_DOMAIN, 0x7fff1f9e9c24)
       = 0 [pid 111638] ioctl(5, DRM_IOCTL_I915_GEM_SET_TILING, 0x7fff1f9e9e40)
       = 0 [pid 111638] getpid()                   = 111638 [pid 111638]
       kcmp(111638, 111638, KCMP_FILE, 6, 5) = 0 [pid 111638] ioctl(5,
       DRM_IOCTL_I915_GEM_SET_TILING, 0x7fff1f9e9ee0) = 0 [pid 111638] ioctl(5,
       DRM_IOCTL_PRIME_HANDLE_TO_FD, 0x7fff1f9e9edc) = 0 [pid 111638] dup(9) =
       10 [pid 111638] poll([{fd=7, events=POLLIN|POLLOUT}], 1, -1) = 1 ([{fd=7,
       revents=POLLIN|POLLOUT}]) [pid 111638] recvmsg(7, {msg_name=NULL,
       msg_namelen=0,
       msg_iov=[{iov_base="\f\0\3\0\0\0\300\4\0\0\0\0\200\2\340\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
       iov_len=4096}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 32
            [pid 111638] writev(7,
       [{iov_base="\224\3\4\0\0\0\300\4\2\0\0\0\0\0\0\0b\0\3\0\4\0\0\0DRI3",
       iov_len=28}], 1) = 28 [pid 111638] poll([{fd=7, events=POLLIN}], 1, -1) =
       1 ([{fd=7, revents=POLLIN}]) [pid 111638] recvmsg(7, {msg_name=NULL,
       msg_namelen=0,
       msg_iov=[{iov_base="\0\3\4\0\2\0\0\0\3\0\224\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
       iov_len=4096}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 32
            [pid 111638] poll([{fd=7, events=POLLIN}], 1, -1) = 1 ([{fd=7,
       revents=POLLIN}]) [pid 111638] recvmsg(7, {msg_name=NULL, msg_namelen=0,
       msg_iov=[{iov_base="\1\0\5\0\0\0\0\0\1\225\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
       iov_len=4096}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 32
            [pid 111638] poll([{fd=7, events=POLLIN|POLLOUT}], 1, -1) = 1
       ([{fd=7, revents=POLLOUT}]) [pid 111638] sendmsg(7, {msg_name=NULL,
       msg_namelen=0,
       msg_iov=[{iov_base="\225\2\6\0\1\0\300\4\0\0\300\4\0\300\22\0\200\2\340\1\0\n\30
       ", iov_len=24}], msg_iovlen=1, msg_control=[{cmsg_len=20,
       cmsg_level=SOL_SOCKET, cmsg_type=SCM_RIGHTS, cmsg_data=[9]}],
       msg_controllen=20, msg_flags=0}, 0) = 24 [pid 111638] close(9)

    */
    printf("************************************\n");
    uint32_t gbm_format = GBM_FORMAT_XRGB8888;
    bufs[i].bo = gbm_bo_create(gbm, win_width, win_height, GBM_FORMAT_XRGB8888,
                                GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT |
                                    GBM_BO_USE_LINEAR);

    if (!bufs[i].bo) {
      fprintf(stderr, "gbm_bo_create failed for %d\n", i);
      assert(false);
    }
    bufs[i].bo_fd = gbm_bo_get_fd(bufs[i].bo);
    if (bufs[i].bo_fd < 0) {
      perror("gbm_bo_get_fd");
      assert(false);
    }

    uint32_t stride = gbm_bo_get_stride(bufs[i].bo);
    uint32_t size_bytes = stride * win_height;
    int test_fd = dup(bufs[i].bo_fd); // to be returned.

    int flags = fcntl(test_fd, F_GETFD);
    if (flags == -1) {
        perror("test_fd invalid in this process");
    } else {
        fprintf(stderr, "test_fd OK (flags=0x%x)\n", flags);
    }

    test_prime_to_handle(test_fd);


    /*
        XCB does not call the ioctl(5, DRM_IOCTL_PRIME_FD_TO_HANDLE,
        0x7ffee2ec01fc)
    */

    bufs[i].pixmap = xcb_generate_id(conn);
    xcb_dri3_pixmap_from_buffer(
      conn, bufs[i].pixmap, win, size_bytes, win_width, win_height, stride,
      24, 32,
            bufs[i].bo_fd); // Takes the ownership of the GPU buffer. and hands over
                        // pixmap as the identifier
    xcb_flush(conn);

    printf("************************************\n");

    /*
        [pid 111638] memfd_create("xshmfence", MFD_CLOEXEC|MFD_ALLOW_SEALING) =
       9 [pid 111638] ftruncate(9, 4)            = 0 [pid 111638] mmap(NULL, 4,
       PROT_READ|PROT_WRITE, MAP_SHARED, 9, 0) = 0x7a2aa2cbf000 [pid 111638]
       poll([{fd=7, events=POLLIN|POLLOUT}], 1, -1) = 1 ([{fd=7,
       revents=POLLOUT}]) [pid 111638] sendmsg(7, {msg_name=NULL, msg_namelen=0,
       msg_iov=[{iov_base="\225\4\4\0\1\0\300\4\2\0\300\4\0\0\0\0",
       iov_len=16}], msg_iovlen=1, msg_control=[{cmsg_len=20,
       cmsg_level=SOL_SOCKET, cmsg_type=SCM_RIGHTS, cmsg_data=[9]}],
       msg_controllen=20, msg_flags=0}, 0) = 16 [pid 111638] close(9) = 0

    */

    uint64_t modifier = gbm_bo_get_modifier(bufs[i].bo);

    // uint64_t modifier = gbm_bo_get_modifier(bufs[i].bo);
    // if (modifier == DRM_FORMAT_MOD_INVALID)
    //   modifier = 0;

    /* Create an xshmfence and register it as an X sync fence for this pixmap */
    bufs[i].shm_fence_fd = xshmfence_alloc_shm(); // ----- (1)
    if (bufs[i].shm_fence_fd < 0) {
      perror("xshmfence_alloc_shm");
      assert(false);
    }
    bufs[i].shm_fence = xshmfence_map_shm(bufs[i].shm_fence_fd);
    if (!bufs[i].shm_fence) {
      fprintf(stderr, "xshmfence_map_shm failed\n");
      assert(false);
    }
    xshmfence_reset(bufs[i].shm_fence); // start unsignaled

    bufs[i].sync_fence = xcb_generate_id(conn);

    xcb_dri3_fence_from_fd_checked(conn, bufs[i].pixmap, bufs[i].sync_fence, 0,
                                   bufs[i].shm_fence_fd);

    /*
            Logic:

                1: Gets the memfd from (1)
                2: Maps to our process using mmap (xshmfence_map_shm)
                3: identifier for the fence is sync_fence (X11 allocated)

                4: Transfers ownership of the fd to the X11. and closes the fd
       inside process.


    */
    xcb_flush(conn);

    printf("----------------\n");
    /*
        This is the idea:

            1: The indented piece of code will be executed at the GAME.
            with the test_fd will be used by the GAME passed from the host.

            it maily calls this ioctl:

            DRM_IOCTL_PRIME_FD_TO_HANDLE

                -> I think this is to establish a GPU HANDLE for the fd passed.
       from egl's PoV
    */

    // fprintf(stderr, "[.] EGL EXTENSIONS: %s\n", eglQueryString(eglDpy, EGL_EXTENSIONS));

    /* Check for the modifier */
    modifier = gbm_bo_get_modifier(bufs[i].bo);
    if (modifier != DRM_FORMAT_MOD_INVALID) {
        fprintf(stderr, "[*] GBM BO %d has modifier: 0x%lx\n", i, modifier);

        EGLint attrs[] = {
            EGL_WIDTH, win_width,
            EGL_HEIGHT, win_height,
            EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_XRGB8888,
            EGL_DMA_BUF_PLANE0_FD_EXT, test_fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
            EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLint)(modifier & 0xffffffffull),
            EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLint)(modifier >> 32),
            EGL_NONE
        };
        bufs[i].image = eglCreateImageKHR_ptr(eglDpy, EGL_NO_CONTEXT,
                                    EGL_LINUX_DMA_BUF_EXT,
                                    NULL, attrs);
        if (bufs[i].image == EGL_NO_IMAGE_KHR) {
            fprintf(stderr,"[X] eglCreateImageKHR failed for buffer %d\n", i); 
            check_egl_error("eglCreateImageKHR"); 
            assert(0);
            return 1;
        }

    } else {
        fprintf(stderr, "[*] GBM BO %d has no modifier\n", i);

        EGLint attrs[] = {
            EGL_WIDTH, win_width,
            EGL_HEIGHT, win_height,
            EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_XRGB8888,
            EGL_DMA_BUF_PLANE0_FD_EXT, test_fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
            EGL_NONE
        };
        bufs[i].image = eglCreateImageKHR_ptr(eglDpy, EGL_NO_CONTEXT,
                                    EGL_LINUX_DMA_BUF_EXT,
                                    NULL, attrs);
        if (bufs[i].image == EGL_NO_IMAGE_KHR) {
            fprintf(stderr,"[X] eglCreateImageKHR failed for buffer %d\n", i); 
            check_egl_error("eglCreateImageKHR"); 
            assert(0);
            return 1;
        }
    }

#if 0
    EGLint attrs[] = {EGL_WIDTH,
            win_width,
            EGL_HEIGHT,
            win_height,
                      EGL_LINUX_DRM_FOURCC_EXT,
                      DRM_FORMAT_XRGB8888,
                      EGL_DMA_BUF_PLANE0_FD_EXT,
                      test_fd,
                      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                      0,
                      EGL_DMA_BUF_PLANE0_PITCH_EXT,
                      stride,
                      EGL_NONE};
    bufs[i].image = eglCreateImageKHR_ptr(eglDpy, EGL_NO_CONTEXT,
                                          EGL_LINUX_DMA_BUF_EXT, NULL, attrs);

    if (bufs[i].image == EGL_NO_IMAGE_KHR) {
      fprintf(stderr, "eglCreateImageKHR failed for buffer %d\n", i);
      check_egl_error("eglCreateImageKHR");
      assert(false);
    }
#endif

    /* Create GL texture and bind the image to it */
    glGenTextures(1, &bufs[i].tex);
    glBindTexture(GL_TEXTURE_2D, bufs[i].tex);
    glEGLImageTargetTexture2DOES_ptr(GL_TEXTURE_2D,
                                     (GLeglImageOES)bufs[i].image);
    glGenRenderbuffers_ptr(1, &bufs[i].rbo_depth);
    glBindRenderbuffer_ptr(GL_RENDERBUFFER, bufs[i].rbo_depth);
    glRenderbufferStorage_ptr(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, win_width,
                win_height);

    /* Create a dedicated FBO for this buffer to avoid per-frame reattachment */
    glGenFramebuffers_ptr(1, &bufs[i].fbo);
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, bufs[i].fbo);
    glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, bufs[i].tex, 0);
    glFramebufferRenderbuffer_ptr(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, bufs[i].rbo_depth);
    GLenum status = glCheckFramebufferStatus_ptr(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "FBO incomplete for buffer %d: 0x%x\n", i, status);
      assert(false);
    }
  }

  glGenFramebuffers_ptr(1, &fbo);
  printf("FB; %d\n", fbo);
}