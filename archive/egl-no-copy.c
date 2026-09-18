// egl_double_zero_copy.c
// Compile:
// gcc -O2 egl_double_zero_copy.c -o egl_double_zero_copy \
//  -lEGL -lGL -lgbm -ldrm -lxcb -lxcb-dri3 -lxcb-present -lxcb-sync -lxshmfence

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/sync.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <math.h>
#include <GL/glext.h>

#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <X11/xshmfence.h>

#define NUM_BUFFERS 2

/* Function pointer globals */
static PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = NULL;
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ptr = NULL;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ptr = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ptr = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_ptr;

static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ptr = NULL;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ptr = NULL;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ptr = NULL;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ptr = NULL;
static PFNGLFENCESYNCPROC glFenceSync_ptr = NULL;
static PFNGLCLIENTWAITSYNCPROC glClientWaitSync_ptr = NULL;
static PFNGLDELETESYNCPROC glDeleteSync_ptr = NULL;

static void check_egl_error(const char *where) {
    EGLint e = eglGetError();
    if (e != EGL_SUCCESS) fprintf(stderr, "EGL error at %s: 0x%04x\n", where, e);
}

int main(void) {
    const int WIDTH = 640, HEIGHT = 480;
    const char *drm_node = "/dev/dri/renderD128";

    /* Open DRM render node and create GBM device */
    int drm_fd = open(drm_node, O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) { perror("open(drm)"); return 1; }
    struct gbm_device *gbm = gbm_create_device(drm_fd);
    if (!gbm) { fprintf(stderr, "gbm_create_device failed\n"); close(drm_fd); return 1; }

    // /* XCB window */
    // xcb_connection_t *conn = xcb_connect(NULL, NULL);
    // if (xcb_connection_has_error(conn)) { fprintf(stderr,"xcb_connect failed\n"); return 1; }
    // xcb_screen_t *screen = (xcb_screen_t*)xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    // xcb_window_t win = xcb_generate_id(conn);
    // uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    // uint32_t values[2] = { screen->black_pixel, XCB_EVENT_MASK_EXPOSURE };
    // xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root,
    //                   0,0, WIDTH, HEIGHT, 0,
    //                   XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
    //                   mask, values);
    // xcb_map_window(conn, win);
    // xcb_flush(conn);
    // // ask for present complete events (optional)
    // xcb_present_select_input(conn, win, XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY, 0);

    /* EGL setup (use gbm device if supported) */
    eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDisplay eglDpy = EGL_NO_DISPLAY;
    if (eglGetPlatformDisplayEXT) eglDpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    if (eglDpy == EGL_NO_DISPLAY) eglDpy = eglGetDisplay((EGLNativeDisplayType)NULL);
    if (!eglDpy) { fprintf(stderr,"eglGetDisplay failed\n"); return 1; }
    if (!eglInitialize(eglDpy, NULL, NULL)) { check_egl_error("eglInitialize"); return 1; }


    EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    EGLConfig cfg; EGLint numCfg;
    if (!eglChooseConfig(eglDpy, cfgAttrs, &cfg, 1, &numCfg) || numCfg == 0) { fprintf(stderr,"eglChooseConfig failed\n"); return 1; }
    if (!eglBindAPI(EGL_OPENGL_API)) { fprintf(stderr,"eglBindAPI failed\n"); return 1; }
    EGLContext ctx = eglCreateContext(eglDpy, cfg, EGL_NO_CONTEXT, NULL);
    if (ctx == EGL_NO_CONTEXT) { check_egl_error("eglCreateContext"); return 1; }
    EGLint pbufAttrs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface pbuf = eglCreatePbufferSurface(eglDpy, cfg, pbufAttrs);
    if (pbuf == EGL_NO_SURFACE) { check_egl_error("eglCreatePbufferSurface"); return 1; }
    if (!eglMakeCurrent(eglDpy, pbuf, pbuf, ctx)) { check_egl_error("eglMakeCurrent"); return 1; }
    /* Load extension entry points */
    eglCreateImageKHR_ptr = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR_ptr = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES_ptr = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    glGenFramebuffers_ptr = (PFNGLGENFRAMEBUFFERSPROC)eglGetProcAddress("glGenFramebuffers");
    glBindFramebuffer_ptr = (PFNGLBINDFRAMEBUFFERPROC)eglGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)eglGetProcAddress("glFramebufferTexture2D");
    glCheckFramebufferStatus_ptr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)eglGetProcAddress("glCheckFramebufferStatus");
    glFenceSync_ptr = (PFNGLFENCESYNCPROC)eglGetProcAddress("glFenceSync");
    glClientWaitSync_ptr = (PFNGLCLIENTWAITSYNCPROC)eglGetProcAddress("glClientWaitSync");
    glDeleteSync_ptr = (PFNGLDELETESYNCPROC)eglGetProcAddress("glDeleteSync");

    if (!eglCreateImageKHR_ptr || !eglDestroyImageKHR_ptr || !glEGLImageTargetTexture2DOES_ptr ||
        !glGenFramebuffers_ptr || !glBindFramebuffer_ptr || !glFramebufferTexture2D_ptr ||
        !glCheckFramebufferStatus_ptr || !glFenceSync_ptr) {
        fprintf(stderr,"Missing required EGL/GL entry points\n"); return 1;
    }

    /* Per-buffer state */
    struct buffer {
        struct gbm_bo *bo;
        int bo_fd;
        xcb_pixmap_t pixmap;
        EGLImageKHR image;
        GLuint tex;
        int shm_fence_fd;
        struct xshmfence *shm_fence;
        xcb_sync_fence_t sync_fence;
    } bufs[NUM_BUFFERS];

    memset(bufs, 0, sizeof(bufs));

    /* Create two GBM BOs, pixmaps, EGLImages and textures */
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        uint32_t gbm_format = GBM_FORMAT_XRGB8888;
        bufs[i].bo = gbm_bo_create(gbm, WIDTH, HEIGHT, gbm_format,
                                  GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
        if (!bufs[i].bo) { fprintf(stderr,"gbm_bo_create failed for %d\n", i); return 1; }
        bufs[i].bo_fd = gbm_bo_get_fd(bufs[i].bo);
        if (bufs[i].bo_fd < 0) { perror("gbm_bo_get_fd"); return 1; }

        uint32_t stride = gbm_bo_get_stride(bufs[i].bo);
        uint32_t size_bytes = stride * HEIGHT;

        bufs[i].pixmap = xcb_generate_id(conn);
        xcb_dri3_pixmap_from_buffer(conn, bufs[i].pixmap, win,
                                    size_bytes, WIDTH, HEIGHT,
                                    stride, 24, 32, bufs[i].bo_fd);
        xcb_flush(conn);

        /* Import pixmap into EGL as EGLImage */
        EGLint imgAttrs[] = { EGL_NONE };
        bufs[i].image = eglCreateImageKHR_ptr(eglDpy, ctx, EGL_NATIVE_PIXMAP_KHR,
                                              (EGLClientBuffer)(uintptr_t)bufs[i].pixmap, imgAttrs);
        if (bufs[i].image == EGL_NO_IMAGE_KHR) {
            fprintf(stderr,"eglCreateImageKHR failed for buffer %d\n", i); check_egl_error("eglCreateImageKHR"); return 1;
        }

        /* Create GL texture and bind the image to it */
        glGenTextures(1, &bufs[i].tex);
        glBindTexture(GL_TEXTURE_2D, bufs[i].tex);
        glEGLImageTargetTexture2DOES_ptr(GL_TEXTURE_2D, (GLeglImageOES)bufs[i].image);

        /* Create an xshmfence and register it as an X sync fence for this pixmap */
        bufs[i].shm_fence_fd = xshmfence_alloc_shm();
        if (bufs[i].shm_fence_fd < 0) { perror("xshmfence_alloc_shm"); return 1; }
        bufs[i].shm_fence = xshmfence_map_shm(bufs[i].shm_fence_fd);
        if (!bufs[i].shm_fence) { fprintf(stderr,"xshmfence_map_shm failed\n"); return 1; }
        xshmfence_reset(bufs[i].shm_fence); // start unsignaled

        bufs[i].sync_fence = xcb_generate_id(conn);
        xcb_dri3_fence_from_fd_checked(conn, bufs[i].pixmap, bufs[i].sync_fence, 0, bufs[i].shm_fence_fd);
        xcb_flush(conn);
    }

    /* Create a single FBO we will re-bind per-frame to the active texture */
    GLuint fbo = 0;
    glGenFramebuffers_ptr(1, &fbo);

    /* Quick check: attach first texture to FBO to confirm setup works */
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bufs[0].tex, 0);
    GLenum status = glCheckFramebufferStatus_ptr(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr,"Initial FBO incomplete: 0x%X\n", status); return 1; }
    glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    printf("double-buffer zero-copy ready. running render loop...\n");

    /* Render loop: ping-pong between buffers */
    for (int frame = 0; frame < 1000; ++frame) {
        int cur = frame % NUM_BUFFERS;

        /* reset fence for this buffer before rendering */
        xshmfence_reset(bufs[cur].shm_fence);

        /* bind FBO -> attach current texture */
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bufs[cur].tex, 0);
        glViewport(0, 0, WIDTH, HEIGHT);

        /* draw */
        float t = frame * 0.01f;
        glClearColor(0.1f + 0.4f * (cur), 0.2f + 0.5f * (0.5f + 0.5f * sinf(t)), 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBegin(GL_TRIANGLES);
          glColor3f(1,0,0); glVertex2f(-0.5f,-0.5f);
          glColor3f(0,1,0); glVertex2f( 0.5f,-0.5f);
          glColor3f(0,0,1); glVertex2f( 0.0f, 0.5f);
        glEnd();
        glFlush();

        // /* GPU sync - ensure GPU finished writing this buffer */
        // GLsync glf = glFenceSync_ptr(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        // glFlush();
        // /* Wait up to 1 second — you can tune/remove this for async path if you export GPU sync FD */
        // glClientWaitSync_ptr(glf, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
        // glDeleteSync_ptr(glf);

        /* signal the xshmfence so X can see the buffer is ready */
        xshmfence_trigger(bufs[cur].shm_fence);

        /* tell X to trigger its sync fence and present the pixmap */
        xcb_sync_trigger_fence(conn, bufs[cur].sync_fence);
        xcb_present_pixmap(conn, win, bufs[cur].pixmap,
                   0,           // serial
                   XCB_NONE,    // valid
                   XCB_NONE,    // update
                   0, 0,        // x, y
                   XCB_NONE,    // target_crtc
                   bufs[cur].sync_fence,  // wait_fence
                   XCB_NONE,               // idle_fence
                   0,           // options
                   0, 0, 0,     // target_msc, divisor, remainder
                   0,           // notifies_len
                   NULL);       // notifies

        xcb_flush(conn);

        /* now detach current texture (optional) */
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

        /* small throttle so humans can see it; remove or adapt for real app */
        // usleep(16000);
    }

    // // /* cleanup (not exhaustive; program will exit) */
    // // for (int i = 0; i < NUM_BUFFERS; ++i) {
    // //     if (bufs[i].shm_fence) { xshmfence_reset(bufs[i].shm_fence); free(bufs[i].shm_fence); }
    // //     if (bufs[i].shm_fence_fd >= 0) close(bufs[i].shm_fence_fd);
    // //     if (bufs[i].image != EGL_NO_IMAGE_KHR) eglDestroyImageKHR_ptr(eglDpy, bufs[i].image);
    // //     if (bufs[i].tex) glDeleteTextures(1, &bufs[i].tex);
    // //     if (bufs[i].bo) { gbm_bo_destroy(bufs[i].bo); bufs[i].bo = NULL; }
    // // }
    // glDeleteFramebuffers_ptr(1, &fbo);
    // xcb_disconnect(conn);
    // gbm_device_destroy(gbm);
    // close(drm_fd);
    // printf("done\n");
    // return 0;
}
