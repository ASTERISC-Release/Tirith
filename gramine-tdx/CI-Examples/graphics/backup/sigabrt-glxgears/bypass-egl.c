#define _GNU_SOURCE

#include "bypass-egl.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/sync.h>
#include <xcb/xcb.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

PFNGLBINDFRAMEBUFFERPROC glClipControl_ptr   = NULL;
PFNGLFENCESYNCPROC glFenceSync_ptr           = NULL;
PFNGLDELETESYNCPROC glDeleteSync_ptr         = NULL;
PFNGLCLIENTWAITSYNCPROC glClientWaitSync_ptr = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ptr;
dlsym_fn_t real_dlsym;
xcb_sync_fence_t prev_present_fence = XCB_NONE;

int cur = 0;

static int setup_syscall_comms(){
    comm_page_t *addr = (char*)mmap((void*)SYS_COMMS_ADDR, SYS_COMMS_SIZE, PROT_READ | PROT_WRITE, (MAP_ANONYMOUS | MAP_FIXED | MAP_PRIVATE), -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap COMMS_REGION");
        return 1;
    }
    memset(addr, 0, SYS_COMMS_SIZE);
    *(volatile uint64_t*)addr = 0x1234567812345678ULL;
    __sync_synchronize();
    sleep(1); // A small sleep to make sure that the sg-listener is truly awake.
    volatile void* data = mmap(NULL, DATA_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "mmap failed, returning...\n");
        perror("mmap DATA_REGION");
        return 1;
    }
    fprintf(stderr, "[*] DATAREGION: 0x%lx; DATASIZE: %ld; ADDR-P1: %p\n", data, DATA_SIZE, addr->p10);
}

void __attribute__((constructor)) sharedgl_entry(void) {
    setup_syscall_comms();  // create_and_setup_xcb_window() - HOST will execute this
    printf("------------------------------------\n");
    const char* drm_node = "/dev/dri/renderD128";
    glClipControl_ptr    = (PFNGLCLIPCONTROLPROC)eglGetProcAddress("glClipControl");
    /* Open DRM render node and create GBM device */
    int drm_fd = open(drm_node, O_RDWR | O_CLOEXEC);  // sent ot the host
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

    fprintf(stderr, "GBM device created successfully!\n");
    cur                = 0;
    prev_present_fence = XCB_NONE;
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
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
        framebuffer = 1;  // overriding it to our FBO
        // printf("Overriding with our FBO\n");
        glBindFramebuffer_ptr(target, framebuffer);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   bufs[cur].tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  bufs[cur].rbo_depth);
        glClipControl_ptr(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
    } else {
        glBindFramebuffer_ptr(target, framebuffer);
        glClipControl_ptr(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[hook] FBO incomplete: 0x%x\n", status);
    }
    // sleep(2000);
}
#include <GL/gl.h>
#include <stdio.h>

void dump_framebuffer_center_pixels(const char *tag, int n_pixels)
{
    unsigned char p[4 * 64]; /* up to 64 pixels */
    if (n_pixels > 64)
        n_pixels = 64;

    /* Query current viewport */
    GLint vp[4]; /* x, y, width, height */
    glGetIntegerv(GL_VIEWPORT, vp);

    GLint cx = vp[0] + vp[2] / 2;
    GLint cy = vp[1] + vp[3] / 2;

    /* Center the read horizontally */
    GLint start_x = cx - n_pixels / 2;
    GLint start_y = cy;

    if (start_x < vp[0])
        start_x = vp[0];

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glFinish();

    glReadPixels(
        start_x,
        start_y,
        n_pixels,
        1,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        p
    );

    fprintf(stderr,
            "%s: center pixels at (%d,%d), n=%d\n",
            tag, start_x, start_y, n_pixels);

    for (int i = 0; i < n_pixels; i++) {
        fprintf(stderr, "%02d: %02x %02x %02x %02x\n",
                i,
                p[i*4 + 0],
                p[i*4 + 1],
                p[i*4 + 2],
                p[i*4 + 3]);
    }

    fprintf(stderr, "\n");
}

void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    fprintf(stderr, "Before glFlush\n");
    fflush(stderr);
    glFlush();
    fprintf(stderr, "After glFlush\n");
    fflush(stderr);
    // dump_framebuffer_center_pixels("FBO CONTENTS", 100);


    fprintf(stderr, "Sent Comm in Swap Buffers\n");
    fflush(stderr);
    comm_page_t* c = comm_page();
    c->p1          = (uint64_t)bufs;
    c->p2          = (uint64_t)cur;
    c->p3          = (uint64_t)prev_present_fence;
    __sync_synchronize();
    c->req_bit     = X11_PRESENT;
    comm_sync_notify(c);

    fprintf(stderr, "Comm done in Swap Buffers\n");
    fflush(stderr);

    prev_present_fence = bufs[cur].sync_fence;
    cur                = (cur + 1) % NUM_BUFFERS;

    fprintf(stderr, "Before glBindFramebuffer\n");
    fflush(stderr);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fprintf(stderr, "After glBindFramebuffer\n");
    fflush(stderr);
}

void glXSwapIntervalEXT(Display* d, GLXDrawable draw, int interval) {
    eglSwapInterval(eglDpy, interval);
}

static Bool (*glXMakeCurrent_ptr)(Display* dpy, GLXDrawable drawable, GLXContext ctx1) = NULL;
Bool glXMakeCurrent(Display* dpy, GLXDrawable drawable, GLXContext ctx1) {
    if (!glXMakeCurrent_ptr) {
        glXMakeCurrent_ptr = dlsym(RTLD_NEXT, "glXMakeCurrent");
        if (!glXMakeCurrent_ptr) {
            fprintf(stderr, "[hook] Failed to resolve real glXMakeCurrent()\n");
            return False;
        }
    }

    setup_egl();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void (*glXGetProcAddressARB(const GLubyte* s))(void) {
    void* addr;
    static void* my_handle;

    /* to-do: use stripped str? */
    if (strstr(s, "glX") || strstr(s, "glBindFramebuffer") || strstr(s, "glXSwapIntervalEXT")) {
        addr = dlsym(NULL, s);
        return addr;
    }

    if (!my_handle) {
        my_handle = dlopen("/opt/mesa/lib/x86_64-linux-gnu/libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    }
    addr = real_dlsym(my_handle, s);
    return addr;
}

void (*glXGetProcAddress(const GLubyte* s))(void) {
    return glXGetProcAddressARB(s);
}
// /* Pretend there are no events */
// xcb_generic_event_t *xcb_poll_for_event(xcb_connection_t *){
//     // fprintf(stderr, "Am I even goign here.\n");
//     return NULL;
// }
// #include <SDL2/SDL.h>
// int SDL_PollEvent(SDL_Event *event)
// {
//     return 0;
// }

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Window wins = -1;
static Window win  = -1;

static const char* glx_extensions =
    "GLX_ARB_create_context GLX_ARB_create_context_no_error GLX_ARB_create_context_profile "
    "GLX_ARB_create_context_robustness GLX_ARB_get_proc_address GLX_EXT_create_context_es2_profile "
    "GLX_EXT_create_context_es_profile GLX_EXT_visual_info";

static int glx_major                 = 1;
static int glx_minor                 = 4;
static const char* glx_majmin_string = "1.4";

static int max_width, max_height, real_width, real_height;
static unsigned long start = 0;
static void* swap_sync_lock;

ICD_SET_MAX_DIMENSIONS_DEFINITION(max_width, max_height, real_width, real_height);
ICD_RESIZE_DEFINITION(real_width, real_height);

struct glx_swap_data {
    XVisualInfo vinfo;
    XVisualInfo* visual_list;
    XVisualInfo visual_template;
    int nxvisuals;
    Window parent;

    int width, height;
    XImage* ximage;
    XEvent event;

    XGCValues gcv;
    GC gc;

    bool initialized;
};

struct glx_fb_config {
    int render_type, drawable_type, double_buffer, red_size, green_size, blue_size, alpha_size,
        stencil_size, depth_size, accum_red_size, accum_green_size, accum_blue_size,
        accum_alpha_size, accum_stencil_size, renderable, visual_type;
    // to-do: add more like stereo
};

static int n_valid_fb_configs;

static struct glx_fb_config fb_configs[1729] = {0};

static int fb_valid_color_sizes[] = {0, 1, 8, 16, 24, 32};

static int fb_valid_render_types[] = {GLX_RGBA_BIT, GLX_COLOR_INDEX_BIT};

static int fb_valid_doublebuffer_types[] = {False, True};

static int fb_valid_drawable_types[] = {GLX_WINDOW_BIT};

static int fb_valid_visual_types[] = {GLX_TRUE_COLOR,   GLX_DIRECT_COLOR, GLX_PSEUDO_COLOR,
                                      GLX_STATIC_COLOR, GLX_GRAY_SCALE,   GLX_STATIC_GRAY};

#define ARR_SIZE(x) (sizeof(x) / sizeof(*x))
static void glx_generate_fb_configs() {
    int idx = 0;
    for (int a = 0; a < ARR_SIZE(fb_valid_color_sizes); a++) {       // varying color size
        for (int b = 0; b < ARR_SIZE(fb_valid_render_types); b++) {  // varying render types
            for (int c = 0; c < ARR_SIZE(fb_valid_doublebuffer_types);
                 c++) {  // varying double buffer
                for (int d = 0; d < ARR_SIZE(fb_valid_drawable_types);
                     d++) {  // varying drawable type
                    for (int e = 0; e < ARR_SIZE(fb_valid_doublebuffer_types);
                         e++) {  // varying renderable
                        for (int f = 0; f < ARR_SIZE(fb_valid_visual_types);
                             f++) {  // varying visual type
                            for (int g = 0; g < ARR_SIZE(fb_valid_color_sizes);
                                 g++) {  // varying depth
                                fb_configs[idx].red_size         = fb_valid_color_sizes[a];
                                fb_configs[idx].green_size       = fb_valid_color_sizes[a];
                                fb_configs[idx].blue_size        = fb_valid_color_sizes[a];
                                fb_configs[idx].alpha_size       = fb_valid_color_sizes[a];
                                fb_configs[idx].accum_red_size   = fb_valid_color_sizes[a];
                                fb_configs[idx].accum_green_size = fb_valid_color_sizes[a];
                                fb_configs[idx].accum_blue_size  = fb_valid_color_sizes[a];
                                fb_configs[idx].accum_alpha_size = fb_valid_color_sizes[a];

                                fb_configs[idx].stencil_size = -1;  // fb_valid_color_sizes[a];
                                fb_configs[idx].depth_size   = fb_valid_color_sizes[g];

                                fb_configs[idx].render_type   = fb_valid_render_types[b];
                                fb_configs[idx].double_buffer = fb_valid_doublebuffer_types[c];
                                fb_configs[idx].drawable_type = fb_valid_drawable_types[d];

                                fb_configs[idx].renderable  = fb_valid_doublebuffer_types[e];
                                fb_configs[idx].visual_type = fb_valid_visual_types[f];

                                idx++;
                            }
                        }
                    }
                }
            }
        }
    }

    n_valid_fb_configs = idx;
}
#undef ARR_SIZE

static const char* glximpl_name_to_string(int name) {
    switch (name) {
        case GLX_VENDOR:
            return "SharedGL";
        case GLX_VERSION:
            return glx_majmin_string;
        case GLX_EXTENSIONS:
            return glx_extensions;
    }
    return "?";
}

GLXContext glXCreateContext(Display* dpy, XVisualInfo* vis, GLXContext share_list, Bool direct) {
    return (GLXContext)1;
}

GLXContext glXGetCurrentContext(void) {
    return (GLXContext)1;
}

void glXDestroyContext(Display* dpy, GLXContext ctx) {}

Bool glXMakeContextCurrent(Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx) {
    return True;
}

Bool glXQueryExtension(Display* dpy, int* errorb, int* event) {
    return True;
}

void glXQueryDrawable(Display* dpy, GLXDrawable draw, int attribute, unsigned int* value) {}

const char* glXQueryExtensionsString(Display* dpy, int screen) {
    return glx_extensions;
}

XVisualInfo *glXChooseVisual(Display *dpy, int screen, int *attrib_list)
{
    XVisualInfo *vinfo = malloc(sizeof(XVisualInfo));
    XMatchVisualInfo(dpy, XDefaultScreen(dpy), 24, TrueColor, vinfo);
    return vinfo;
}

GLXContext glXCreateNewContext(Display* dpy, GLXFBConfig config, int render_type,
                               GLXContext share_list, Bool direct) {
    return (GLXContext)1;
}

Display* glXGetCurrentDisplay(void) {
    return XOpenDisplay(NULL);
}

Bool glXQueryVersion(Display* dpy, int* maj, int* min) {
    *maj = glx_major;
    *min = glx_minor;
    return True;
}

const char* glXGetClientString(Display* dpy, int name) {
    return glximpl_name_to_string(name);
}

GLXWindow glXCreateWindow(Display* dpy, GLXFBConfig config, Window win, const int* attrib_list) {
    win = win;
    return win;
}

void glXDestroyWindow(Display* dpy, GLXWindow win) {}

int glXGetFBConfigAttrib(Display* dpy, GLXFBConfig config, int attribute, int* value) {
    unsigned int index = (int)(size_t)config;
    if (index > n_valid_fb_configs) {
        fprintf(stderr, "glXGetFBConfigAttrib : attempted access on config %u, only %u exist\n",
                index, n_valid_fb_configs);
        return Success;
    }

    // fprintf(stderr, "glXGetFBConfigAttrib : access on config %u\n", index);

    struct glx_fb_config fb_config = fb_configs[index];

    switch (attribute) {
        case GLX_FBCONFIG_ID:
            *value = index;
            break;
        case GLX_RENDER_TYPE:
            *value |= fb_config.render_type;  // GLX_RGBA_BIT;
            break;
        case GLX_VISUAL_ID:
            *value = XDefaultVisual(dpy, 0)->visualid;  // fb_config.visual_type;
            break;
        case GLX_BUFFER_SIZE:
            *value = 32;
            break;
        case GLX_SAMPLES:
            *value = 0;
            break;
        case GLX_SAMPLE_BUFFERS:
            *value = 1;
            break;
        case GLX_DRAWABLE_TYPE:
            *value |= fb_config.drawable_type;  // GLX_WINDOW_BIT;
            break;
        case GLX_DOUBLEBUFFER:
            *value = fb_config.double_buffer;
            break;
        case GLX_RED_SIZE:
            *value = fb_config.red_size;
            break;
        case GLX_GREEN_SIZE:
            *value = fb_config.green_size;
            break;
        case GLX_BLUE_SIZE:
            *value = fb_config.blue_size;
            break;
        case GLX_ALPHA_SIZE:
            *value = fb_config.alpha_size;
            break;
        case GLX_STENCIL_SIZE:
            *value = 0;  // fb_config.stencil_size;
            break;
        case GLX_ACCUM_RED_SIZE:
            *value = fb_config.accum_red_size;
            break;
        case GLX_ACCUM_GREEN_SIZE:
            *value = fb_config.accum_green_size;
            break;
        case GLX_ACCUM_BLUE_SIZE:
            *value = fb_config.accum_blue_size;
            break;
        case GLX_ACCUM_ALPHA_SIZE:
            *value = fb_config.accum_alpha_size;  // 8;
            break;
        case GLX_DEPTH_SIZE:
            *value = 24;  // fb_config.depth_size; // 24;
            break;
        case GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB:
            *value = 1;
            break;
    }

    return Success;
}

GLXFBConfig* glXGetFBConfigs(Display* dpy, int screen, int* nelements) {
    GLXFBConfig* fb_config = calloc(1, sizeof(GLXFBConfig) * n_valid_fb_configs);
    for (int i = 0; i < n_valid_fb_configs; i++) fb_config[i] = (GLXFBConfig)((size_t)i);
    *nelements = n_valid_fb_configs;
    return fb_config;
}

XVisualInfo* glXGetVisualFromFBConfig(Display* dpy, GLXFBConfig config) {
    XVisualInfo* vinfo = malloc(sizeof(XVisualInfo));
    XMatchVisualInfo(dpy, XDefaultScreen(dpy), 24, TrueColor, vinfo);
    return vinfo;
}

GLXContext glXCreateContextAttribsARB(Display* dpy, GLXFBConfig config, GLXContext share_context,
                                      Bool direct, const int* attrib_list) {
    /*
     * probably not needed, stuck around for testing
     */
    int* attribs = (int*)attrib_list;
    while (*attribs) {
        int attrib = *attribs++;
        int value  = *attribs++;

        if (attrib == GLX_CONTEXT_PROFILE_MASK_ARB &&
            value == GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB)
            return NULL;
    }

    return glXCreateContext(dpy, NULL, 0, 0);
}

int glXGetConfig(Display* dpy, XVisualInfo* visual, int attrib, int* value) {
    return 1;
}

GLXFBConfig* glXChooseFBConfig(Display* dpy, int screen, const int* attrib_list, int* nitems) {
    return glXGetFBConfigs(dpy, screen, nitems);
}

Bool glXIsDirect(Display* dpy, GLXContext ctx) {
    return True;
}

const char* glXQueryServerString(Display* dpy, int screen, int name) {
    return glximpl_name_to_string(name);
}

void glXCopyContext(Display* dpy, GLXContext src, GLXContext dst, unsigned long mask) {}

GLXPixmap glXCreateGLXPixmap(Display* dpy, XVisualInfo* visual, Pixmap pixmap) {
    return (GLXPixmap)1;
}

void glXDestroyGLXPixmap(Display* dpy, GLXPixmap pixmap) {}

GLXDrawable glXGetCurrentDrawable(void) {
    return win;
}

void glXUseXFont(Font font, int first, int count, int list) {}

void glXWaitGL(void) {}

void glXWaitX(void) {}

int glXQueryContext(Display* dpy, GLXContext ctx, int attribute, int* value) {
    return Success;
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

    if (symbol && strstr(symbol, "glXChooseVisual")) {
        fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
        // TODO: return your own replacement if needed
        return glXChooseVisual;
    }

    if (symbol && strstr(symbol, "glXCreateContext")) {
        // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
        // TODO: return your own replacement if needed
        return glXCreateContext;
    }

    if (symbol && strstr(symbol, "glBindFramebuffer")) {
        // fprintf(stderr, "[HOOK] dlsym for %s intercepted -> %p\n", symbol, sym);
        // TODO: return your own replacement if needed
        return glBindFramebuffer;
    }
    return sym;
}
