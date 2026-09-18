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
#define NUM_BUFFERS 2
#define XCB_DRI3_PIXMAP_SCANOUT (1<<0)
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ptr = NULL;

struct gl_state {
  GLint viewport[4];
  GLint draw_fbo, read_fbo;
  GLint tex_binding;
  GLint active_tex;
  GLint read_buffer, draw_buffer;
};

static void save_gl_state(struct gl_state *s) {
  glGetIntegerv(GL_VIEWPORT, s->viewport);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s->draw_fbo);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s->read_fbo);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->tex_binding);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &s->active_tex);
  glGetIntegerv(GL_READ_BUFFER, &s->read_buffer);
  glGetIntegerv(GL_DRAW_BUFFER, &s->draw_buffer);
}

static void restore_gl_state(const struct gl_state *s) {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, s->read_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s->draw_fbo);
  glActiveTexture(s->active_tex);
  glBindTexture(GL_TEXTURE_2D, s->tex_binding);
  glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
  glReadBuffer(s->read_buffer);
  glDrawBuffer(s->draw_buffer);
}


/* Function pointer globals */
static PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = NULL;
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ptr = NULL;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ptr = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ptr = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_ptr;
PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT_ptr = NULL;
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ptr = NULL;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ptr = NULL;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ptr = NULL;
static PFNGLFENCESYNCPROC glFenceSync_ptr = NULL;
static PFNGLCLIENTWAITSYNCPROC glClientWaitSync_ptr = NULL;
static PFNGLDELETESYNCPROC glDeleteSync_ptr = NULL;
PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUF_ptr = NULL;

static void check_egl_error(const char *where) {
    EGLint e = eglGetError();
    if (e != EGL_SUCCESS) fprintf(stderr, "EGL error at %s: 0x%04x\n", where, e);
}

int main(void) {
    const int WIDTH = 800, HEIGHT = 600;
    const char *drm_node = "/dev/dri/renderD128";

    /* Open DRM render node and create GBM device */
    int drm_fd = open(drm_node, O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) { perror("open(drm)"); return 1; }
    struct gbm_device *gbm = gbm_create_device(drm_fd);
    if (!gbm) { fprintf(stderr, "gbm_create_device failed\n"); close(drm_fd); return 1; }
        struct gbm_surface *gbm_surface = gbm_surface_create(
    gbm, WIDTH, HEIGHT,
    GBM_FORMAT_ARGB8888,
    GBM_BO_USE_RENDERING  | GBM_BO_USE_SCANOUT
);
    eglQueryDevicesEXT_ptr = (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
if (!eglQueryDevicesEXT_ptr) {
    fprintf(stderr, "eglQueryDevicesEXT not available\n");
    return 1;
}
    EGLDeviceEXT devices[16];
    EGLint num_devices = 0;
    eglQueryDevicesEXT_ptr(16, devices, &num_devices);
    // /* XCB window */
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) { fprintf(stderr,"xcb_connect failed\n"); return 1; }
    xcb_screen_t *screen = (xcb_screen_t*)xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_window_t win = xcb_generate_id(conn);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = { screen->black_pixel, XCB_EVENT_MASK_EXPOSURE };
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root,
                      0,0, WIDTH, HEIGHT, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                      mask, values);
    xcb_map_window(conn, win);
    xcb_flush(conn);
    // ask for present complete events (optional)
    xcb_present_select_input(conn, win, XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY, 0);

    /* EGL setup (use gbm device if supported) */
    
    eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDisplay eglDpy = EGL_NO_DISPLAY;
    if (eglGetPlatformDisplayEXT)  eglDpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm, NULL);

    if (eglDpy == EGL_NO_DISPLAY) {eglDpy = eglGetDisplay((EGLNativeDisplayType)NULL); printf("No displ\n"); return -1; }
    if (!eglDpy) { fprintf(stderr,"eglGetDisplay failed\n"); return 1; }
    if (!eglInitialize(eglDpy, NULL, NULL)) { check_egl_error("eglInitialize"); return 1; }

        EGLint major, minor;
    eglInitialize(eglDpy, &major, &minor);
    // printf("EGL init: %d.%d\n", major, minor);
    // printf("Vendor: %s\n", eglQueryString(eglDpy, EGL_VENDOR));
    // printf("Extensions: %s\n", eglQueryString(eglDpy, EGL_EXTENSIONS));

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

    // Bind desktop GL API
    if (!eglBindAPI(EGL_OPENGL_API)) { fprintf(stderr,"eglBindAPI failed\n"); return 1; }

    // Create context with no surface
    EGLContext ctx = eglCreateContext(eglDpy, cfg, EGL_NO_CONTEXT, NULL);
    if (ctx == EGL_NO_CONTEXT) { check_egl_error("eglCreateContext"); return 1; }
    EGLSurface surf = eglCreateWindowSurface(eglDpy, cfg, gbm_surface, NULL);
    // Make current with no surface (surfaceless)
    if (!eglMakeCurrent(eglDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        check_egl_error("eglMakeCurrent");
        return 1;
    }
    // /* Load extension entry points */
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
                [pid 111638] poll([{fd=7, events=POLLIN|POLLOUT}], 1, -1) = 1 ([{fd=7, revents=POLLIN|POLLOUT}])
                [pid 111638] recvmsg(7, {msg_name=NULL, msg_namelen=0, msg_iov=[{iov_base="\f\0\3\0\0\0\300\4\0\0\0\0\200\2\340\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", iov_len=4096}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 32
                [pid 111638] writev(7, [{iov_base="\224\3\4\0\0\0\300\4\2\0\0\0\0\0\0\0b\0\3\0\4\0\0\0DRI3", iov_len=28}], 1) = 28
                [pid 111638] poll([{fd=7, events=POLLIN}], 1, -1) = 1 ([{fd=7, revents=POLLIN}])
                [pid 111638] recvmsg(7, {msg_name=NULL, msg_namelen=0, msg_iov=[{iov_base="\0\3\4\0\2\0\0\0\3\0\224\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", iov_len=4096}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 32
                [pid 111638] poll([{fd=7, events=POLLIN}], 1, -1) = 1 ([{fd=7, revents=POLLIN}])
                [pid 111638] recvmsg(7, {msg_name=NULL, msg_namelen=0, msg_iov=[{iov_base="\1\0\5\0\0\0\0\0\1\225\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", iov_len=4096}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 32
                [pid 111638] poll([{fd=7, events=POLLIN|POLLOUT}], 1, -1) = 1 ([{fd=7, revents=POLLOUT}])
                [pid 111638] sendmsg(7, {msg_name=NULL, msg_namelen=0, msg_iov=[{iov_base="\225\2\6\0\1\0\300\4\0\0\300\4\0\300\22\0\200\2\340\1\0\n\30 ", iov_len=24}], msg_iovlen=1, msg_control=[{cmsg_len=20, cmsg_level=SOL_SOCKET, cmsg_type=SCM_RIGHTS, cmsg_data=[9]}], msg_controllen=20, msg_flags=0}, 0) = 24
                [pid 111638] close(9)   
        
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


                                /*
                                        XCB does not call the ioctl(5, DRM_IOCTL_PRIME_FD_TO_HANDLE, 0x7ffee2ec01fc)
                                */

                            bufs[i].pixmap = xcb_generate_id(conn);
                            xcb_dri3_pixmap_from_buffer(conn, bufs[i].pixmap, win,
                                                        size_bytes, WIDTH, HEIGHT,
                                                        stride, 24, 32, bufs[i].bo_fd); //Takes the ownership of the GPU buffer. and hands over pixmap as the identifier
                            xcb_flush(conn);

                            printf("************************************\n");

                            /*
                                [pid 111638] memfd_create("xshmfence", MFD_CLOEXEC|MFD_ALLOW_SEALING) = 9
                                [pid 111638] ftruncate(9, 4)            = 0
                                [pid 111638] mmap(NULL, 4, PROT_READ|PROT_WRITE, MAP_SHARED, 9, 0) = 0x7a2aa2cbf000
                                [pid 111638] poll([{fd=7, events=POLLIN|POLLOUT}], 1, -1) = 1 ([{fd=7, revents=POLLOUT}])
                                [pid 111638] sendmsg(7, {msg_name=NULL, msg_namelen=0, msg_iov=[{iov_base="\225\4\4\0\1\0\300\4\2\0\300\4\0\0\0\0", iov_len=16}], msg_iovlen=1, msg_control=[{cmsg_len=20, cmsg_level=SOL_SOCKET, cmsg_type=SCM_RIGHTS, cmsg_data=[9]}], msg_controllen=20, msg_flags=0}, 0) = 16
                                [pid 111638] close(9)                   = 0
                            
                            */
                            
                            uint64_t modifier = gbm_bo_get_modifier(bufs[i].bo);

                            // uint64_t modifier = gbm_bo_get_modifier(bufs[i].bo);
                            if (modifier == DRM_FORMAT_MOD_INVALID) modifier = 0;

                            /* Create an xshmfence and register it as an X sync fence for this pixmap */
                            bufs[i].shm_fence_fd = xshmfence_alloc_shm(); // ----- (1)
                            if (bufs[i].shm_fence_fd < 0) { perror("xshmfence_alloc_shm"); return 1; }
                            bufs[i].shm_fence = xshmfence_map_shm(bufs[i].shm_fence_fd);
                            if (!bufs[i].shm_fence) { fprintf(stderr,"xshmfence_map_shm failed\n"); return 1; }
                            xshmfence_reset(bufs[i].shm_fence); // start unsignaled

                            bufs[i].sync_fence = xcb_generate_id(conn);

                            xcb_dri3_fence_from_fd_checked(conn, bufs[i].pixmap, bufs[i].sync_fence, 0, bufs[i].shm_fence_fd);

                            /*
                                    Logic:

                                        1: Gets the memfd from (1)
                                        2: Maps to our process using mmap (xshmfence_map_shm)
                                        3: identifier for the fence is sync_fence (X11 allocated)

                                        4: Transfers ownership of the fd to the X11. and closes the fd inside process.
                                        
                            
                            */
                            xcb_flush(conn);
                            

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
        printf("----------------\n");
        
    }

    /* Create a single FBO we will re-bind per-frame to the active texture */
    GLuint fbo;
    glGenFramebuffers_ptr(1, &fbo);
    printf("FB; %d\n", fbo);

    glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bufs[0].tex, 0);
    GLenum status = glCheckFramebufferStatus_ptr(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr,"Initial FBO incomplete: 0x%X\n", status); return 1; }
    glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    struct gl_state st;

    printf("double-buffer zero-copy ready. running render loop...\n");
    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.0f);

    
    xcb_sync_fence_t prev_present_fence = XCB_NONE;
    /* Render loop: ping-pong between buffers */
    for (int frame = 0; frame < 1; ++frame) {
        int cur = frame % NUM_BUFFERS;
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bufs[cur].tex, 0);
        xshmfence_reset(bufs[cur].shm_fence);

        glViewport(0, 0, WIDTH, HEIGHT);

        // Animate background color
        float r = 0.3f + 0.3f * sinf(frame * 1.2f);
        float g = 0.3f + 0.3f * sinf(frame * 0.9f + 1.0f);
        float b = 0.3f + 0.3f * sinf(frame * 1.3f + 2.0f);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw a simple RGB triangle
        glBegin(GL_TRIANGLES);
            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex2f(-0.6f, -0.6f);

            glColor3f(0.0f, 1.0f, 0.0f);
            glVertex2f( 0.6f, -0.6f);

            glColor3f(0.0f, 0.0f, 1.0f);
            glVertex2f( 0.0f,  0.6f);
        glEnd();

        /*
            [pid 124684] ioctl(5, DRM_IOCTL_SYNCOBJ_WAIT, 0x7ffc48fc78d0) = -1 ETIME (Timer expired)
            [pid 124684] ioctl(5, DRM_IOCTL_SYNCOBJ_WAIT, 0x7ffc48fc78d0) = 0  
        */
        // /* GPU sync - ensure GPU finished writing this buffer */
        glFlush();
        GLsync glf = glFenceSync_ptr(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        /* Wait up to 1 second — you can tune/remove this for async path if you export GPU sync FD */
        GLenum val = glClientWaitSync_ptr(glf, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000);
        printf("Value for enum: %d\n", val);
        glDeleteSync_ptr(glf);
        printf("++++++++++++++++++++\n");
        // save_gl_state(&st);
        // sleep(2);

        /* bind FBO -> attach current texture */
        // glBlitFramebuffer(0,0,WIDTH,HEIGHT, 0,0,WIDTH,HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        

        // glBindFramebuffer_ptr(GL_DRAW_FRAMEBUFFER, 0);
        // glFramebufferTexture2D_ptr(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

        printf("&&&&&&&&&&&&&&&&&&&&&&&\n");

        

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
                   prev_present_fence,               // idle_fence
                   0,           // options
                   0, 0, 0,     // target_msc, divisor, remainder
                   0,           // notifies_len
                   NULL);       // notifies

        xcb_flush(conn);
        // restore_gl_state(&st);
        // glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        prev_present_fence = bufs[cur].sync_fence;

        /* now detach current texture (optional) */
        // glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

        /* small throttle so humans can see it; remove or adapt for real app */
        // usleep(16000);
    }
    // sleep(20);
    printf("Here.\n");

    return 0;
}
