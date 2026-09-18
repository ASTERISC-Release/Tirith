#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <drm/drm_fourcc.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#define WIDTH 1920
#define HEIGHT 1080
#define DEFAULT_STRIDE (WIDTH*4)
#define SOCK_PATH "/tmp/sharedgl.sock"

/* --- EGL extension prototypes --- */
static PFNEGLCREATEIMAGEKHRPROC            p_eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC           p_eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC p_glEGLImageTargetTexture2DOES;

/* --- slot info --- */
struct slot {
    int in_use;
    int fd;
    int w, h, stride;
    EGLImageKHR image;
    GLuint tex;
} slots[2];

/* --- basic X11 + EGL init --- */
static Display *x_dpy;
static Window   x_win;
static EGLDisplay egl_dpy;
static EGLSurface egl_surf;
static EGLContext egl_ctx;

static void egl_init_x11(void)
{
    x_dpy = XOpenDisplay(NULL);
    if (!x_dpy) { perror("XOpenDisplay"); exit(1); }

    int scr = DefaultScreen(x_dpy);
    x_win = XCreateSimpleWindow(x_dpy, RootWindow(x_dpy, scr),
                                0, 0, WIDTH, HEIGHT, 1,
                                BlackPixel(x_dpy, scr), WhitePixel(x_dpy, scr));
    XStoreName(x_dpy, x_win, "EGL X11 Receiver");
    XMapWindow(x_dpy, x_win);

    egl_dpy = eglGetDisplay((EGLNativeDisplayType)x_dpy);
    if (egl_dpy == EGL_NO_DISPLAY) { fprintf(stderr,"eglGetDisplay failed\n"); exit(1); }
    if (!eglInitialize(egl_dpy, NULL, NULL)) { fprintf(stderr,"eglInitialize failed\n"); exit(1); }
    eglBindAPI(EGL_OPENGL_API);

    EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    EGLConfig cfg; EGLint n;
    eglChooseConfig(egl_dpy, cfg_attr, &cfg, 1, &n);
    egl_surf = eglCreateWindowSurface(egl_dpy, cfg, (EGLNativeWindowType)x_win, NULL);
    egl_ctx  = eglCreateContext(egl_dpy, cfg, EGL_NO_CONTEXT, NULL);
    eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx);

    p_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    p_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    p_glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    fprintf(stderr, "EGL extensions: %s\n", eglQueryString(egl_dpy, EGL_EXTENSIONS));
}

static void import_dmabuf(int idx, int fd, int w, int h, int stride,
                          uint32_t fourcc, uint64_t modifier)
{
    if (slots[idx].in_use) return;

    EGLint attrs[32];
    int n = 0;
    attrs[n++] = EGL_WIDTH;  attrs[n++] = w;
    attrs[n++] = EGL_HEIGHT; attrs[n++] = h;
    attrs[n++] = EGL_LINUX_DRM_FOURCC_EXT; attrs[n++] = fourcc;
    attrs[n++] = EGL_DMA_BUF_PLANE0_FD_EXT; attrs[n++] = fd;
    attrs[n++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT; attrs[n++] = 0;
    attrs[n++] = EGL_DMA_BUF_PLANE0_PITCH_EXT; attrs[n++] = stride;

    // Only include modifier if it’s valid (not linear)
    if (modifier != DRM_FORMAT_MOD_INVALID && modifier != 0) {
        attrs[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        attrs[n++] = (EGLint)(modifier & 0xffffffff);
        attrs[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        attrs[n++] = (EGLint)(modifier >> 32);
        fprintf(stderr, "[HOST] slot%d: using modifier 0x%llx\n",
                idx, (unsigned long long)modifier);
    }

    attrs[n++] = EGL_NONE;

    EGLImageKHR img = p_eglCreateImageKHR(egl_dpy, EGL_NO_CONTEXT,
                                          EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
    if (img == EGL_NO_IMAGE_KHR) {
        fprintf(stderr, "EGLImage import failed for slot %d (err=0x%x)\n",
                idx, eglGetError());
        return;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    p_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    slots[idx].in_use = 1;
    slots[idx].fd = fd;
    slots[idx].w = w;
    slots[idx].h = h;
    slots[idx].stride = stride;
    slots[idx].image = img;
    slots[idx].tex = tex;

    fprintf(stderr,
        "[HOST] slot%d: fd=%d w=%d h=%d stride=%d fourcc=%.4s modifier=0x%llx\n",
        idx, fd, w, h, stride, (char*)&fourcc, (unsigned long long)modifier);
}


/* --- draw fullscreen quad --- */
static void draw_quad(GLuint tex)
{
    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(0,0); glVertex2f(-1,-1);
      glTexCoord2f(1,0); glVertex2f( 1,-1);
      glTexCoord2f(0,1); glVertex2f(-1, 1);
      glTexCoord2f(1,1); glVertex2f( 1, 1);
    glEnd();

    eglSwapBuffers(egl_dpy, egl_surf);
}

// Include format and modifier info from sender
struct init_msg {
    int frame_index;
    int width;
    int height;
    int stride;
    uint32_t fourcc;
    uint64_t modifier; // NEW
};


static int recv_fd_and_init(int c)
{
    struct msghdr msg = {0};
    char ctrl[CMSG_SPACE(sizeof(int))];
    struct init_msg im = {0};
    struct iovec iov = { .iov_base = &im, .iov_len = sizeof(im) };
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    msg.msg_control = ctrl; msg.msg_controllen = sizeof(ctrl);

    ssize_t r = recvmsg(c, &msg, 0);
    if (r <= 0) return -1;

    int fd = -1;
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
        memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    if (fd < 0) return -1;

    int idx = im.frame_index & 1;
    if (im.width <= 0) im.width = WIDTH;
    if (im.height <= 0) im.height = HEIGHT;
    if (im.stride <= 0) im.stride = DEFAULT_STRIDE;

    uint32_t fourcc = im.fourcc ? im.fourcc : DRM_FORMAT_XRGB8888;
    uint64_t modifier = im.modifier;
    import_dmabuf(idx, fd, im.width, im.height, im.stride, fourcc, modifier);

    draw_quad(slots[idx].tex);
    return 0;
}

static int recv_index_only(int c)
{
    int idx=0;
    if (read(c,&idx,sizeof(idx))!=sizeof(idx)) return -1;
    idx &= 1;
    if (!slots[idx].in_use) return -1;
    draw_quad(slots[idx].tex);
    return 0;
}
static volatile int *frame_idx_ptr = NULL;
static int shm_fd = -1;
static int shm_ready = 0;
// OLD signature (remove this version)
// static void recv_and_map_shm_fd(void)

// Replace your recv_and_map_shm_fd(int srv_fd) with this:

static void recv_and_map_shm_fd(int srv_fd)
{
    int c = accept(srv_fd, NULL, NULL);
    if (c < 0) { perror("accept shmfd"); return; }

    // read 1 byte payload + scm_rights
    unsigned char byte;
    struct iovec iov = { .iov_base = &byte, .iov_len = 1 };

    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    struct msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);  // IMPORTANT: full buffer size

    ssize_t n;
again:
    n = recvmsg(c, &msg, 0);
    if (n < 0) {
        if (errno == EINTR) goto again;
        perror("recvmsg shmfd");
        close(c);
        return;
    }
    if (n == 0) {  // peer closed without sending payload -> no SCM_RIGHTS delivered
        fprintf(stderr, "recvmsg shmfd: EOF without payload\n");
        close(c);
        return;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS ||
        cmsg->cmsg_len < CMSG_LEN(sizeof(int))) {
        fprintf(stderr, "[HOST] invalid/absent SCM_RIGHTS for shmfd\n");
        close(c);
        return;
    }

    memcpy(&shm_fd, CMSG_DATA(cmsg), sizeof(int));
    close(c);

    frame_idx_ptr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (frame_idx_ptr == MAP_FAILED) {
        perror("mmap shm_fd");
        shm_fd = -1;
        return;
    }

    shm_ready = 1;
    fprintf(stderr, "[HOST] mapped shared index shm_fd=%d\n", shm_fd);
}


int main(void)
{
    egl_init_x11();

    unlink(SOCK_PATH);
    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un a = {0};
    a.sun_family = AF_UNIX;
    strncpy(a.sun_path, SOCK_PATH, sizeof(a.sun_path) - 1);
    bind(srv, (struct sockaddr*)&a, sizeof(a));
    listen(srv, 4);
    fprintf(stderr, "[HOST] Listening on %s\n", SOCK_PATH);

    int phase = 0;   // 0 = waiting for shm_fd
                     // 1 = waiting for first BO
                     // 2 = waiting for second BO
                     // 3 = steady state (polling shared mem)


for (;;) {
    if (phase < 3) {
        // accept one connection for each setup step
        if (phase == 0) {
            recv_and_map_shm_fd(srv);   // ⬅️ now accepts here
            if (shm_ready) phase = 1;
            continue;
        } else if (phase == 1) {
            int c = accept(srv, NULL, NULL);
            if (c >= 0) { if (recv_fd_and_init(c) == 0) phase = 2; close(c); }
            continue;
        } else if (phase == 2) {
            int c = accept(srv, NULL, NULL);
            if (c >= 0) { if (recv_fd_and_init(c) == 0) phase = 3; close(c); }
            continue;
        }
    }

    if (shm_ready && frame_idx_ptr) {
    int cur = *frame_idx_ptr;

    if (cur == 0 || cur == 1) {
        draw_quad(slots[cur].tex);
        eglSwapBuffers(egl_dpy, egl_surf);

        // mark frame as consumed
        *frame_idx_ptr = -1;
        __sync_synchronize(); // ensure visibility
    }

    usleep(1000); // light polling
}



    while (XPending(x_dpy)) { XEvent e; XNextEvent(x_dpy, &e); }
    usleep(1000);
}
}
