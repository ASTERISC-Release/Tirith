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
#include <sys/socket.h>
#include <sys/un.h>

#include <X11/xshmfence.h>
xcb_sync_fence_t prev_present_fence = XCB_NONE;
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
#define NUM_BUFFERS 2
#define XCB_DRI3_PIXMAP_SCANOUT (1<<0)
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ptr = NULL;

struct gbm_surface *gbm_surface;
struct gbm_device *gbm;
#define WIDTH 800
#define HEIGHT 600
struct init_msg {
    int frame_index;
    int width;
    int height;
    int stride;
    uint32_t fourcc;
    uint64_t modifier;
};

xcb_connection_t *conn;
xcb_window_t win;
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
        GLuint rbo_depth;
    } bufs[NUM_BUFFERS];
int cur = 0;
    GLuint fbo;
static void check_egl_error(const char *where) {
    EGLint e = eglGetError();
    if (e != EGL_SUCCESS) fprintf(stderr, "EGL error at %s: 0x%04x\n", where, e);
}
typedef void (*PFNGLCLIPCONTROLPROC)(GLenum origin, GLenum depth);
static PFNGLCLIPCONTROLPROC glClipControl_ptr = NULL;
#define GL_UPPER_LEFT 0x8CA2
#define GL_ZERO_TO_ONE 0x935F
#include <sys/mman.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include <fcntl.h>

static int shm_fd = -1;
static volatile int *frame_idx_ptr = NULL;
static int shm_ready = 0;

static void init_shared_index(void)
{
    shm_fd = memfd_create("sgl_index", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (shm_fd < 0) {
        perror("[SGL] memfd_create");
        return;
    }

    if (ftruncate(shm_fd, sizeof(int)) < 0) {
        perror("[SGL] ftruncate");
        close(shm_fd);
        return;
    }

    frame_idx_ptr = mmap(NULL, sizeof(int),
                         PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (frame_idx_ptr == MAP_FAILED) {
        perror("[SGL] mmap");
        close(shm_fd);
        return;
    }

    *frame_idx_ptr = -1;
    shm_ready = 1;

    const char *sock_path = "/tmp/sharedgl.sock";
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) {
        perror("[SGL] socket");
        return;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[SGL] connect shm_fd");
        close(s);
        return;
    }

    unsigned char marker = 0xA5;
struct iovec iov = { .iov_base = &marker, .iov_len = 1 };

char cmsgbuf[CMSG_SPACE(sizeof(int))];
struct msghdr msg = {0};
msg.msg_iov = &iov;
msg.msg_iovlen = 1;
msg.msg_control = cmsgbuf;
msg.msg_controllen = sizeof(cmsgbuf);  // IMPORTANT

struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
cmsg->cmsg_level = SOL_SOCKET;
cmsg->cmsg_type  = SCM_RIGHTS;
cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
memcpy(CMSG_DATA(cmsg), &shm_fd, sizeof(int));

if (sendmsg(s, &msg, 0) < 0) {
    perror("[SGL] sendmsg shm_fd");
} else {
    fprintf(stderr, "[SGL] Sent shared index shm_fd=%d to receiver\n", shm_fd);
}

// optional: graceful shutdown to ensure receiver sees the message before close
shutdown(s, SHUT_WR);
usleep(20000);  // 20 ms grace
close(s);
}



void __attribute__((constructor)) sharedgl_entry(void) 
{

    const char *drm_node = "/dev/dri/renderD128";
    glClipControl_ptr = (PFNGLCLIPCONTROLPROC)eglGetProcAddress("glClipControl");
    /* Open DRM render node and create GBM device */
    int drm_fd = open(drm_node, O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) { perror("open(drm)"); return;}
    gbm = gbm_create_device(drm_fd);
    if (!gbm) { fprintf(stderr, "gbm_create_device failed\n"); close(drm_fd); return ; }
    init_shared_index();

}
static void send_frame_index(int index)
{
    if (!shm_ready || frame_idx_ptr == NULL)
        return;

    *frame_idx_ptr = index;
    __sync_synchronize();  // memory barrier
    // *frame_idx_ptr = -1;
}

static void send_bo_fd(int frame_index, int fd, int width, int height,
                       int stride, uint32_t fourcc, uint64_t modifier)
{
    const char *sock_path = "/tmp/sharedgl.sock";

    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) {
        perror("[SGL] socket");
        return;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[SGL] connect");
        close(s);
        return;
    }

    struct init_msg msg = {
        .frame_index = frame_index,
        .width = width,
        .height = height,
        .stride = stride,
        .fourcc = fourcc,
        .modifier = modifier
    };

    struct msghdr msgh = {0};
    struct iovec iov = { .iov_base = &msg, .iov_len = sizeof(msg) };
    char cmsgbuf[CMSG_SPACE(sizeof(int))];

    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = cmsgbuf;
    msgh.msg_controllen = sizeof(cmsgbuf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));

    *((int *)CMSG_DATA(cmsg)) = fd;

    ssize_t sent = sendmsg(s, &msgh, 0);
    if (sent < 0)
        perror("[SGL] sendmsg");
    else
        fprintf(stderr, "[SGL] Frame %d BO FD %d sent (stride=%d, mod=0x%llx)\n",
                frame_index, fd, stride, (unsigned long long)modifier);

    close(s);
}


static int send_fd_over_unix_socket(int fd_to_send) {
    const char *sock_path = "/tmp/sharedgl.sock";   // must exist on host receiver
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket(AF_UNIX)");
        return -1;
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return -1;
    }

    struct msghdr msg = {0};
    struct iovec iov;
    char buf[CMSG_SPACE(sizeof(int))];
    memset(buf, 0, sizeof(buf));

    // Dummy payload: frame index
    int frame_index = cur;
    iov.iov_base = &frame_index;
    iov.iov_len = sizeof(frame_index);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // Set control message to send the FD
    msg.msg_control = buf;
    msg.msg_controllen = CMSG_SPACE(sizeof(int));
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *((int *) CMSG_DATA(cmsg)) = fd_to_send;

    if (sendmsg(sock_fd, &msg, 0) < 0) {
        perror("sendmsg");
        close(sock_fd);
        return -1;
    }

    close(sock_fd);
    return 0;
}


typedef void (*glXSwapBuffers_t)(Display *, GLXDrawable);
static glXSwapBuffers_t real_glXSwapBuffers = NULL;
void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
        /*
            Send the fd over a shm to another process via socket
        */
        // glFinish();
        int fd_to_send = bufs[cur].bo_fd;
        // Track how many FDs we've sent
        static int sent_fd_count = 0;

        if (sent_fd_count < 2) {
            // First two frames: send fd normally
            fprintf(stderr, "[SGL] Sending BO fd=%d for frame=%d\n", fd_to_send, cur);
            uint32_t fourcc = GBM_FORMAT_XRGB8888;
            uint32_t stride = gbm_bo_get_stride(bufs[cur].bo);
            uint64_t modifier = gbm_bo_get_modifier(bufs[cur].bo);
            send_bo_fd(cur, fd_to_send, WIDTH, HEIGHT, stride, fourcc, modifier);
            
        } else {
            send_frame_index(cur);
            usleep(100);
        }
        sent_fd_count++;
        // cur++;
        cur = (cur+1) % NUM_BUFFERS;
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bufs[cur].tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                            GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, bufs[cur].rbo_depth);
                            glClipControl_ptr(GL_UPPER_LEFT, GL_ZERO_TO_ONE);



}
EGLDisplay eglDpy;
typedef void *(*dlsym_fn_t)(void *, const char *);

static dlsym_fn_t real_dlsym = NULL;
static Bool (*glXMakeCurrent_ptr)(Display *dpy, GLXDrawable drawable, GLXContext ctx1) = NULL;
Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx1)
{
    // printf("hellooooo.\n");
    eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    eglDpy = EGL_NO_DISPLAY;
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
    // EGLSurface surf = eglCreateWindowSurface(eglDpy, cfg, gbm_surface, NULL);
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
                                GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);

                            if (!bufs[i].bo) { fprintf(stderr,"gbm_bo_create failed for %d\n", i); return 1; }
                            bufs[i].bo_fd = gbm_bo_get_fd(bufs[i].bo);
                            if (bufs[i].bo_fd < 0) { perror("gbm_bo_get_fd"); return 1; }

                            uint32_t stride = gbm_bo_get_stride(bufs[i].bo);
                            uint32_t size_bytes = stride * HEIGHT;

                            int test_fd = dup(bufs[i].bo_fd); //to be returned.


                                /*
                                        XCB does not call the ioctl(5, DRM_IOCTL_PRIME_FD_TO_HANDLE, 0x7ffee2ec01fc)
                                */

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
        
    }

    glGenFramebuffers_ptr(1, &fbo);
    printf("FB; %d\n", fbo);

    glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bufs[0].tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                          GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_RENDERBUFFER, bufs[cur].rbo_depth);
    glClipControl_ptr(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
        GLenum status = glCheckFramebufferStatus_ptr(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr,"Initial FBO incomplete: 0x%X\n", status); return 1; }
    return true;
}


// Your LD_PRELOAD replacement
void glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    // Lazy resolve the real symbol once
    if (!glBindFramebuffer_ptr) {
        glBindFramebuffer_ptr = real_dlsym(RTLD_NEXT, "glBindFramebuffer");
        if (!glBindFramebuffer_ptr) {
            fprintf(stderr, "[hook] Failed to resolve real glBindFramebuffer()\n");
            return;
        }
    }

    // --- Your custom behavior here ---
    fprintf(stderr, "[hook] glBindFramebuffer(target=0x%x, framebuffer=%u)\n",
            target, framebuffer);

    // Forward to the real function

    if(framebuffer == 0){
        framebuffer = 1; //overriding it to our FBO
        printf("Overriding with our FBO\n");
        glBindFramebuffer_ptr(target, framebuffer);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bufs[cur].tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                          GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_RENDERBUFFER, bufs[cur].rbo_depth);
        glClipControl_ptr(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
    }
    else {
        glBindFramebuffer_ptr(target, framebuffer);
        glClipControl_ptr(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    }
}

void glXSwapIntervalEXT(Display *d, GLXDrawable draw, int interval){
    eglSwapInterval(eglDpy, interval);
}
static void *my_handle;
void (*glXGetProcAddressARB(const GLubyte *s))(void)
{
    void *addr;
    // printf("heyyyy @ glXGetProcAddressARB: %s\n", s);
    /* to-do: use stripped str? */
    if (strstr(s, "glX") || strstr(s, "glBindFramebuffer") || strstr(s, "glXSwapIntervalEXT") ) {
        addr = dlsym(NULL, s);
        return addr;
    }
    

    if (!my_handle) {
        my_handle = dlopen("/opt/mesa/lib/x86_64-linux-gnu/libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    }
    addr = real_dlsym(my_handle, s);
    return addr;
}

void (*glXGetProcAddress(const GLubyte *s))(void)
{
    // printf("heyyyy @ glXGetProcAddress\n");
    return glXGetProcAddressARB(s);
}

void* dlsym(void* handle, const char* symbol) {
    if (!real_dlsym) {
        real_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
        if (!real_dlsym) {
            fprintf(stderr, "[hook] Failed to resolve real real_dlsym()\n");
            return False;
        }
    }

    void* sym = real_dlsym(handle, symbol);
    if (symbol && strstr(symbol, "SwapBuffers")) {
        // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
        // TODO: return your own replacement if needed
        return glXSwapBuffers;
    }
    if (symbol && strstr(symbol, "MakeCurrent")) {
        // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
        // TODO: return your own replacement if needed
        return glXMakeCurrent;
    }

    if (symbol && strstr(symbol, "glXGetProcAddress")) {
        // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
        // TODO: return your own replacement if needed
        return glXGetProcAddress;
    }

    if (symbol && strstr(symbol, "glXSwapIntervalEXT")) {
        // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
        // TODO: return your own replacement if needed
        return glXSwapIntervalEXT;
    }



    if (symbol && strstr(symbol, "glBindFramebuffer")) {
        // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
        // TODO: return your own replacement if needed
        return glBindFramebuffer;
    }
    return sym;
}
