#define _GNU_SOURCE
#define GL_GLEXT_PROTOTYPES
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
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/sync.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <math.h>
#include <GL/glext.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/xfixes.h>

#include <X11/xshmfence.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>

#include <X11/xshmfence.h>
#include "bypass-egl.h"
#include <GL/glext.h>
check bufs[NUM_BUFFERS];
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ptr = NULL;
PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ptr ;
PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ptr ;
struct gbm_device *gbm;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ptr ;
GLuint fbo;
EGLDisplay eglDpy;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ptr ;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ptr ;

int setup_egl()
{
    eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)eglGetProcAddress("glFramebufferTexture2D");
    glFenceSync_ptr = (PFNGLFENCESYNCPROC)eglGetProcAddress("glFenceSync");
    glClientWaitSync_ptr = (PFNGLCLIENTWAITSYNCPROC)eglGetProcAddress("glClientWaitSync");
    glDeleteSync_ptr = (PFNGLDELETESYNCPROC)eglGetProcAddress("glDeleteSync");
    eglDpy = EGL_NO_DISPLAY;
        fprintf(stderr, "EGL func intd successfully!\n");

    if (eglGetPlatformDisplayEXT)  eglDpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm, NULL);

        fprintf(stderr, "EGL Display created successfully!\n");

    if (eglDpy == EGL_NO_DISPLAY) {eglDpy = eglGetDisplay((EGLNativeDisplayType)NULL); fprintf(stderr, "No displ\n"); return -1; }
    if (!eglDpy) { fprintf(stderr,"eglGetDisplay failed\n"); return 1; }
    if (!eglInitialize(eglDpy, NULL, NULL)) { check_egl_error("eglInitialize"); return 1; }

        EGLint major, minor;
    eglInitialize(eglDpy, &major, &minor);

        EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_DONT_CARE,    // don’t request pbuffer
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLConfig cfg;
    EGLint numCfg = 0;
    if (!eglChooseConfig(eglDpy, cfgAttrs, &cfg, 1, &numCfg) || numCfg == 0) {
        fprintf(stderr,"eglChooseConfig failed\n");
        return 1;
    }

    fprintf(stderr, "EGL choose config created successfully!\n");
    // Bind desktop GL API
    if (!eglBindAPI(EGL_OPENGL_API)) { fprintf(stderr,"eglBindAPI failed\n"); return 1; }
    fprintf(stderr,"EGL Bound API successfully %p!\n", eglCreateContext);

    // Create context with no surface
    const EGLint context_attributes[] = {
    EGL_CONTEXT_MAJOR_VERSION, 2,  // Set to 2
    EGL_CONTEXT_MINOR_VERSION, 1,  // Set to 1
    EGL_NONE // Terminator (Profile mask omitted for simplicity/robustness)
};
    EGLContext ctx = eglCreateContext(eglDpy, NULL, EGL_NO_CONTEXT, context_attributes);

    fprintf(stderr, "EGL CTX created successfully!\n");
    if (ctx == EGL_NO_CONTEXT) { check_egl_error("eglCreateContext"); return 1; }
    // Make current with no surface (surfaceless)
    if (!eglMakeCurrent(eglDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        check_egl_error("eglMakeCurrent");
        return 1;
    }

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


    memset(bufs, 0, sizeof(bufs));
    // sleep(10);
    /* Create two GBM BOs, pixmaps, EGLImages and textures */
    for (int i = 0; i < NUM_BUFFERS; ++i) {

        /*
            This is the idea:

                the following indented snippet will be executed at the HOST. and it will return a FD (int)

                IOCTLs made:

                [pid 111638] ioctl(5, DRM_IOCTL_I915_GEM_CREATE_EXT, 0x7fff1f9e9c08) = 0
                [pid 111638] ioctl(5, DRM_IOCTL_I915_GEM_SET_DOMAIN, 0x7fff1f9e9c24) = 0
                [pid 111638] ioctl(5, DRM_IOCTL_I915_GEM_SET_TILING, 0x7fff1f9e9e40) = 0
                [pid 111638] getpid()                   = 111638
                [pid 111638] kcmp(111638, 111638, KCMP_FILE, 6, 5) = 0
                [pid 111638] ioctl(5, DRM_IOCTL_I915_GEM_SET_TILING, 0x7fff1f9e9ee0) = 0
                [pid 111638] ioctl(5, DRM_IOCTL_PRIME_HANDLE_TO_FD, 0x7fff1f9e9edc) = 0
                [pid 111638] dup(9)                     = 10
        
        */
        printf("************************************\n");
        uint32_t gbm_format = GBM_FORMAT_XRGB8888;
        bufs[i].bo = gbm_bo_create(gbm, WIDTH, HEIGHT, GBM_FORMAT_XRGB8888,
            GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
        if (!bufs[i].bo) { fprintf(stderr,"gbm_bo_create failed for %d\n", i); return 1; }
        bufs[i].bo_fd = gbm_bo_get_fd(bufs[i].bo);
        if (bufs[i].bo_fd < 0) { perror("gbm_bo_get_fd"); return 1; }
        uint32_t stride = gbm_bo_get_stride(bufs[i].bo);
        uint32_t size_bytes = stride * HEIGHT;
        int test_fd = dup(bufs[i].bo_fd); //to be returned.
        
        comm_page_t* c = comm_page();
        c->p1 = (uint64_t) bufs;
        c->p2 = (uint64_t) i;
        c->p3 = (uint64_t) size_bytes;
        c->p4 = (uint64_t) stride;
        c->req_bit = X11_SETUP;
        sleep(1); /* TODO: Debug this later!, without this, the x-11 setup welcome msg just appears once */
        comm_sync_notify(c);
                        
        printf("----------------\n");
        /*
            This is the idea:

                1: The indented piece of code will be executed at the GAME. 
                with the test_fd will be used by the GAME passed from the host.

                it maily calls this ioctl:

                DRM_IOCTL_PRIME_FD_TO_HANDLE

                    -> I think this is to establish a GPU HANDLE for the fd passed. from egl's PoV
        */
        EGLint attrs[] = {
                    EGL_WIDTH, WIDTH,
                    EGL_HEIGHT, HEIGHT,
                    EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_XRGB8888,
                    EGL_DMA_BUF_PLANE0_FD_EXT, test_fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
                    EGL_NONE
                };
        bufs[i].image = eglCreateImageKHR_ptr(eglDpy, EGL_NO_CONTEXT,
                                    EGL_LINUX_DMA_BUF_EXT,
                                    NULL,
                                    attrs);
        if (bufs[i].image == EGL_NO_IMAGE_KHR) {
            fprintf(stderr,"eglCreateImageKHR failed for buffer %d\n", i); check_egl_error("eglCreateImageKHR"); return 1;
        }
        /* Create GL texture and bind the image to it */
        glGenTextures(1, &bufs[i].tex);
        glBindTexture(GL_TEXTURE_2D, bufs[i].tex);
        glEGLImageTargetTexture2DOES_ptr(GL_TEXTURE_2D, (GLeglImageOES)bufs[i].image);
        glGenRenderbuffers(1, &bufs[i].rbo_depth);
        glBindRenderbuffer(GL_RENDERBUFFER, bufs[i].rbo_depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, WIDTH, HEIGHT);
        // sleep(2000);
    }

    glGenFramebuffers_ptr(1, &fbo);
    fprintf(stderr, "FB; %d\n", fbo);
}
