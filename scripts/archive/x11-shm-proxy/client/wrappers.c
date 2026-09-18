/* wrappers.c --- X11 function wrappers for IPC client  ---  -*- C -*- 
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "common.h"

/* Add a function to handle unsupported calls */
static pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Add at the top with other globals */
static int unsupported_call_made = 0;

/* Fake display instances */
static Display fake_display_instance = {0};
static Screen fake_screen_instance = {0};

/* runtime pointers */
extern shared_region_t *g;
extern pid_t mypid;
extern uint32_t seq_counter;

/* helper: is IPC ready? */
static int ipc_ready(void) { return (g != NULL); }

#ifndef XVisualIDFromVisual
typedef unsigned long VisualID; /* ensure this typedef exists */
#define XVisualIDFromVisual(v)  ((VisualID)(uintptr_t)(v))
#endif

static void handle_unsupported_call(const char *function_name) {
    pthread_mutex_lock(&exit_mutex);
    if (!unsupported_call_made) {
        unsupported_call_made = 1;
        fprintf(stderr, "[ipc-client] ERROR: Unsupported X11 call '%s' - exiting client\n", function_name);
        pthread_mutex_unlock(&exit_mutex);
        
        /* Try to cleanup gracefully */
        if (g) {
            munmap((void*)g, sizeof(*g));
            g = NULL;
        }
        
        /* Exit the process */
        _exit(127); /* Use _exit to avoid atexit handlers that might call X11 functions */
    }
    pthread_mutex_unlock(&exit_mutex);
}

/* Add a macro for unsupported functions */
#define UNSUPPORTED_CALL() handle_unsupported_call(__func__)

/* manage local Display* pointers */
Display* localDisplayPointers[NUM_DISPLAYS];
int numLocalDisplays = 0;
Display* get_local_display_pointer(void) {
    for (int i = 0; i < NUM_DISPLAYS; i++) {
        if (localDisplayPointers[i] == NULL) {
            localDisplayPointers[i] = malloc(4096); // Allocate memory for Display
            return localDisplayPointers[i];
        }
    }
    fprintf(stderr, "[ipc-client] ERROR: Exceeded maximum number of local Display pointers (%d)\n", NUM_DISPLAYS);
    return NULL; // No more available slots
}
void put_local_display_pointer(Display* dpy) {
    for (int i = 0; i < NUM_DISPLAYS; i++) {
        if (localDisplayPointers[i] == dpy) {
            localDisplayPointers[i] = NULL;
            numLocalDisplays--;
            return;
        }
    }
    fprintf(stderr, "[ipc-client] WARNING: Attempted to remove unknown Display pointer %p\n", (void*)dpy);
}

/* minimal placeholder for Visual in client process */
typedef struct {
    VisualID visualid;
    /* You may add extra bookkeeping fields if desired. */
} VisualPlaceholder;

/* simple vector of placeholders (leak-minimizing free at XCloseDisplay recommended) */
#define MAX_PLACEHOLDERS 256
static VisualPlaceholder *visual_placeholders[MAX_PLACEHOLDERS];
static int visual_placeholders_count = 0;

/* helper to allocate placeholder and return as Visual* */
static Visual *make_visual_placeholder(VisualID vid) {
    if (visual_placeholders_count >= MAX_PLACEHOLDERS) return NULL;
    VisualPlaceholder *vp = calloc(1, sizeof(*vp));
    if (!vp) return NULL;
    vp->visualid = vid;
    visual_placeholders[visual_placeholders_count++] = vp;
    return (Visual*)(uintptr_t)vp;
}

/* optional cleanup you can call at XCloseDisplay time */
static void ipc_free_visual_placeholders(void) {
    for (int i = 0; i < visual_placeholders_count; ++i) {
        free(visual_placeholders[i]);
        visual_placeholders[i] = NULL;
    }
    visual_placeholders_count = 0;
}


/* XOpenDisplay: open local Display* and request server to open server-side display */
Display *XOpenDisplay(const char *display_name) {
    /* Create a fake Display structure */
    Display *local = &fake_display_instance;
    
    /* Initialize the fake display */
    local->fd = 1000; /* Fake file descriptor */
    local->default_screen = 0; /* Always 0 as requested */
    local->nscreens = 1;
    local->screens = &fake_screen_instance;
    
    /* Initialize the fake screen */
    local->screens[0].width = 1920;
    local->screens[0].height = 1080;
    local->screens[0].mwidth = 508;
    local->screens[0].mheight = 285;
    local->screens[0].root = 0x123; /* Fake root window ID */   

    fprintf(stderr, "[ipc-client] XOpenDisplay: requesting server to open display '%s'\n",
            display_name ? display_name : "(null)");

    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XOpenDisplay;
    req.client_handle = (uint64_t)(uintptr_t)local;
    if (display_name) strncpy(req.p.open_display.name, display_name, sizeof(req.p.open_display.name)-1);
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        fprintf(stderr, "[ipc-client] XOpenDisplay: server opened (ret=%" PRIu64 ")\n", resp.ret_handle);
    } else {
        fprintf(stderr, "[ipc-client] XOpenDisplay: server open failed\n");
    }
    return local;
}

int XDefaultScreen(Display *display) {
    (void)display; /* Suppress unused parameter warning */

    /* Always return 0 as requested */
    fprintf(stderr, "[ipc-client] XDefaultScreen: returning 0\n");
    return 0;
}

/* XCloseDisplay: close local and request server close mapped display */
int XCloseDisplay(Display *display) {
    int r = 0;
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XCloseDisplay;
    req.client_handle = (uint64_t)(uintptr_t)display;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        fprintf(stderr, "[ipc-client] XCloseDisplay: server closed mapped display\n");
    } else {
        fprintf(stderr, "[ipc-client] XCloseDisplay: server close failed\n");
    }

    /* Free any allocated Visual placeholders */
    ipc_free_visual_placeholders(); 

    return r;
}

/* XCreateSimpleWindow: forward to server and return server Window value */
Window XCreateSimpleWindow(Display *display, Window root, int x, int y, unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border, unsigned long bg) {
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XCreateSimpleWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.create_window_simple.x = x; req.p.create_window_simple.y = y;
    req.p.create_window_simple.w = width; req.p.create_window_simple.h = height;
    req.p.create_window_simple.border = border_width;
    req.p.create_window_simple.border_px = border;
    req.p.create_window_simple.bg = bg;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        return (Window)resp.ret_handle;
    }
    return (Window)0;
}

/* New: XCreateWindow (full) */
Window XCreateWindow(Display *display, Window parent, int x, int y,
                     unsigned int width, unsigned int height, unsigned int border_width,
                     int depth, unsigned int class, Visual *visual,
                     unsigned long valuemask, XSetWindowAttributes *attributes) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XCreateWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.create_window.parent = (uint64_t)(uintptr_t)parent;
    req.p.create_window.x = x;
    req.p.create_window.y = y;
    req.p.create_window.width = width;
    req.p.create_window.height = height;
    req.p.create_window.border_width = border_width;
    req.p.create_window.depth = depth;
    req.p.create_window.class = class;
    req.p.create_window.visual = (uint64_t)(uintptr_t)visual;
    req.p.create_window.valuemask = valuemask;

    if (attributes) {
        if (valuemask & CWBackPixel) req.p.create_window.background_pixel = attributes->background_pixel;
        if (valuemask & CWBorderPixel) req.p.create_window.border_pixel = attributes->border_pixel;
        if (valuemask & CWEventMask) req.p.create_window.event_mask = attributes->event_mask;
    } else {
        req.p.create_window.background_pixel = 0;
        req.p.create_window.border_pixel = 0;
        req.p.create_window.event_mask = 0;
    }

    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        return (Window)resp.ret_handle;
    }
    return (Window)0;
}

/* XMapWindow: forward */
int XMapWindow(Display *display, Window w) {
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XMapWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.simple_window.window_handle = (uint64_t)w;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return (int)0;
}

/* New: XUnmapWindow */
int XUnmapWindow(Display *display, Window w) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XUnmapWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.unmap_window.window_handle = (uint64_t)w;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return (int)0;
}

/* New: XConfigureWindow */
int XConfigureWindow(Display *display, Window w, unsigned int value_mask, XWindowChanges *changes) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XConfigureWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.configure_window.window_handle = (uint64_t)w;
    req.p.configure_window.value_mask = value_mask;
    if (changes) {
        /* copy fields: only common ones are stored */
        req.p.configure_window.x = changes->x;
        req.p.configure_window.y = changes->y;
        req.p.configure_window.width = changes->width;
        req.p.configure_window.height = changes->height;
        req.p.configure_window.border_width = changes->border_width;
        req.p.configure_window.sibling = (uint64_t)(uintptr_t)changes->sibling;
        req.p.configure_window.stack_mode = changes->stack_mode;
    } else {
        req.p.configure_window.x = req.p.configure_window.y = 0;
        req.p.configure_window.width = req.p.configure_window.height = 0;
        req.p.configure_window.border_width = 0;
        req.p.configure_window.sibling = 0;
        req.p.configure_window.stack_mode = 0;
    }
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return (int)0;
}

/* XDestroyWindow: forward */
int XDestroyWindow(Display *display, Window w) {
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XDestroyWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.simple_window.window_handle = (uint64_t)w;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return (int)0;
}

/* XFlush: forward */
int XFlush(Display *display) {
    ipc_slot_t req; memset(&req,0,sizeof(req));
    req.op = OP_XFlush;
    req.client_handle = (uint64_t)(uintptr_t)display;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return (int)0;
}

/* New: XSelectInput */
int XSelectInput(Display *display, Window w, long event_mask) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XSelectInput;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.select_input.window_handle = (uint64_t)w;
    req.p.select_input.event_mask = (unsigned long)event_mask;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
    return (int)0;
}

/* XPending: check local event ring count */
int XPending(Display *display) {
    return event_count(); // Use your helper function
}

/* XNextEvent: pop from shared event ring, rewrite display pointer */
int XNextEvent(Display *display, XEvent *event_return) {
    XEvent ev;
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return 0;
    while (g->evt_ring.head == g->evt_ring.tail) {
        pthread_cond_wait(&g->evt_cond, &g->evt_mtx);
    }
    ev = g->evt_ring.events[g->evt_ring.head];
    g->evt_ring.head = (g->evt_ring.head + 1) % g->evt_ring.cap;
    pthread_mutex_unlock(&g->evt_mtx);

    /* rewrite */
    ev.xany.display = display;
    *event_return = ev;
    return 0;
}

int XMoveWindow(Display *display, Window w, int x, int y) {
    /* Use XConfigureWindow to move the window */
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XConfigureWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.configure_window.window_handle = (uint64_t)w;
    req.p.configure_window.value_mask = CWX | CWY;
    req.p.configure_window.x = x;
    req.p.configure_window.y = y;
    req.p.configure_window.width = 0;
    req.p.configure_window.height = 0;
    req.p.configure_window.border_width = 0;
    req.p.configure_window.sibling = 0;
    req.p.configure_window.stack_mode = 0;
    
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;

    fprintf(stderr, "[ERROR] XMoveWindow: Should not come here.\n");
    return 0;
}

int XResizeWindow(Display *display, Window w, unsigned int width, unsigned int height) {
    /* Use XConfigureWindow to resize the window */
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XConfigureWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.configure_window.window_handle = (uint64_t)w;
    req.p.configure_window.value_mask = CWWidth | CWHeight;
    req.p.configure_window.x = 0;
    req.p.configure_window.y = 0;
    req.p.configure_window.width = width;
    req.p.configure_window.height = height;
    req.p.configure_window.border_width = 0;
    req.p.configure_window.sibling = 0;
    req.p.configure_window.stack_mode = 0;
    
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0) return resp.status==0 ? 0 : 1;
   
    fprintf(stderr, "[ERROR] XResizeWindow: Should not come here.\n");
    return 0;
}

Window XDefaultRootWindow(Display *display) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XDefaultRootWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        return (Window)resp.ret_handle;
    }
    /* failure -> return 0 */
    return (Window)0;    
}

Window XRootWindow(Display *display, int screen_number) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XRootWindow;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.root_window.screen = screen_number;
    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) == 0 && resp.status == 0) {
        return (Window)resp.ret_handle;
    }
    /* failure -> return 0 */
    return (Window)0;    
}

/* XMatchVisualInfo wrapper */
int XMatchVisualInfo(Display *display, int screen, int depth, int class, XVisualInfo *vinfo) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XMatchVisualInfo;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.match_visual.screen = screen;
    req.p.match_visual.depth = depth;
    req.p.match_visual.c_class = (unsigned int)class;

    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) != 0) {
        /* RPC failed */
        return 0; /* as XMatchVisualInfo: 0 -> not found / error */
    }

    if (!resp.p.match_visual.matched) {
        /* no match */
        return 0;
    }

    /* matched: fill vinfo if caller provided one */
    if (vinfo) {
        vinfo->visual = NULL; /* server-side Visual* is not valid in client process */
        vinfo->visualid = resp.p.match_visual.visualid;
        vinfo->screen = screen;
        vinfo->depth = resp.p.match_visual.depth_ret;
        /* use the same name your XVisualInfo uses; if field is 'c_class', convert */
//#ifdef HAVE_C_CLASS_FIELD_IN_XVISUALINFO
#if 1
        vinfo->c_class = resp.p.match_visual.c_class_ret;
#else
        vinfo->class = resp.p.match_visual.c_class_ret; /* if your struct uses 'class' */
#endif
        vinfo->red_mask = resp.p.match_visual.red_mask;
        vinfo->green_mask = resp.p.match_visual.green_mask;
        vinfo->blue_mask = resp.p.match_visual.blue_mask;
        vinfo->colormap_size = resp.p.match_visual.colormap_size;
        vinfo->bits_per_rgb = resp.p.match_visual.bits_per_rgb;
    }

    return resp.ret_int ? 1 : 0;
}

/* Client: XGetVisualInfo wrapper */
XVisualInfo *XGetVisualInfo(Display *display, long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XGetVisualInfo;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.get_visuals.vinfo_mask = vinfo_mask;

    /* copy template fields if provided */
    if (vinfo_template) {
        if (vinfo_mask & VisualScreenMask) req.p.get_visuals.screen = vinfo_template->screen;
        if (vinfo_mask & VisualDepthMask)  req.p.get_visuals.depth = vinfo_template->depth;
        if (vinfo_mask & VisualClassMask)  req.p.get_visuals.c_class = vinfo_template->c_class;
        if (vinfo_mask & VisualIDMask)     req.p.get_visuals.visualid = vinfo_template->visualid;
        if (vinfo_mask & VisualRedMask)    req.p.get_visuals.red_mask = vinfo_template->red_mask;
        if (vinfo_mask & VisualGreenMask)  req.p.get_visuals.green_mask = vinfo_template->green_mask;
        if (vinfo_mask & VisualBlueMask)   req.p.get_visuals.blue_mask = vinfo_template->blue_mask;
        if (vinfo_mask & VisualColormapSizeMask) req.p.get_visuals.colormap_size = vinfo_template->colormap_size;
        if (vinfo_mask & VisualBitsPerRGBMask)   req.p.get_visuals.bits_per_rgb = vinfo_template->bits_per_rgb;
    }

    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) != 0) {
        if (nitems_return) *nitems_return = 0;
        return NULL;
    }

    int received = resp.p.get_visuals.nitems;
    if (received <= 0) {
        if (nitems_return) *nitems_return = 0;
        return NULL;
    }

    /* allocate XVisualInfo array for the caller */
    XVisualInfo *out = calloc(received, sizeof(XVisualInfo));
    if (!out) {
        if (nitems_return) *nitems_return = 0;
        return NULL;
    }

    for (int i = 0; i < received; ++i) {
        // out[i].visual = NULL; /* server Visual* cannot be used in client process */
        out[i].visual = (Visual*)(uintptr_t)resp.p.get_visuals.list[i].visualid;
        out[i].visualid = resp.p.get_visuals.list[i].visualid;
        out[i].screen = resp.p.get_visuals.list[i].screen;
        out[i].depth = resp.p.get_visuals.list[i].depth;
        out[i].c_class = resp.p.get_visuals.list[i].c_class;
        out[i].red_mask = resp.p.get_visuals.list[i].red_mask;
        out[i].green_mask = resp.p.get_visuals.list[i].green_mask;
        out[i].blue_mask = resp.p.get_visuals.list[i].blue_mask;
        out[i].colormap_size = resp.p.get_visuals.list[i].colormap_size;
        out[i].bits_per_rgb = resp.p.get_visuals.list[i].bits_per_rgb;
    }

    if (nitems_return) *nitems_return = received;
    return out;
}

/* Wrapper: XDefaultVisual(display, screen) */
Visual *XDefaultVisual(Display *display, int screen) {
    ipc_slot_t req;
    memset(&req, 0, sizeof(req));
    req.op = OP_XDefaultVisual;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.default_visual.screen = screen;

    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) != 0) {
        /* RPC transport failure */
        return NULL;
    }
    if (resp.status != 0) {
        /* server-side failure */
        return NULL;
    }

    // VisualID vid = (VisualID)resp.p.default_visual.visualid;
    //
    // /* Create a client-side placeholder Visual* to return.
    //    Users should avoid dereferencing this pointer; instead,
    //    call XGetVisualInfo or use the visualid. */
    // Visual *v = make_visual_placeholder(vid);
    // return v;
    VisualID vid = (VisualID)resp.ret_handle; /* server returned vid */
    return (Visual*)(uintptr_t)vid;  /* placeholder Visual* encoding visualid */
}

VisualID ipc_visualid_from_placeholder(Visual *v) {
    return v ? ((VisualPlaceholder*)(uintptr_t)v)->visualid : (VisualID)0;
}

/* CLIENT: XCreateImage (client-local lightweight allocator)  */
XImage *XCreateImage(Display *display,
                     Visual *visual, /* unused in our client allocator, but kept for API */
                     unsigned int depth,
                     int format,
                     int offset, /* unused */
                     char *data,
                     unsigned int width,
                     unsigned int height,
                     int bitmap_pad,
                     int bytes_per_line) {

    /* Allocate XImage struct and backing buffer if data == NULL */
    size_t needed = (size_t)(bytes_per_line) * (size_t)height;
    XImage *xi = calloc(1, sizeof(XImage));
    if (!xi) return NULL;

    xi->width = width;
    xi->height = height;
    xi->depth = depth;
    xi->format = format;
    xi->bits_per_pixel = (bitmap_pad > 0) ? bitmap_pad : 32; /* best effort */
    xi->byte_order = LSBFirst; /* client-endian assumption; not critical */
    xi->bitmap_unit = 8;
    xi->bitmap_bit_order = LSBFirst;
    xi->bytes_per_line = bytes_per_line;

    if (data) {
        xi->data = data;
        /* mark so free should not free data buffer (we assume caller owns it) */
    } else {
        xi->data = malloc(needed ? needed : 1);
        if (!xi->data) { free(xi); return NULL; }
        /* zero to be safe */
        memset(xi->data, 0, needed);
    }

    /* Note: xi->visual and other fields are intentionally left NULL/0; use XGetVisualInfo
       to get a real XVisualInfo when necessary. */
    xi->obdata = NULL;
    return xi;
}

/* CLIENT: XDestroyImage (when client created XImage) - free client-owned buffers */
int XDestroyImage(XImage *ximage) {
    if (!ximage) return 0;
    if (ximage->data) free(ximage->data);
    free(ximage);
    return 0;
}

/* --- Add XPutImage client-side helper/wrapper --- */
static pthread_mutex_t putimage_lock = PTHREAD_MUTEX_INITIALIZER;

/* XPutImage: copy image rectangle into shared region and issue RPC to server.
   Signature follows standard XPutImage returning int (0 = success, non-zero = failure). */
int XPutImage(Display *display, Drawable d, GC gc, XImage *image,
              int src_x, int src_y, int dst_x, int dst_y,
              unsigned int width, unsigned int height) {
    if (!ipc_ready()) { UNSUPPORTED_CALL(); return 1; }
    if (!image || !image->data) return 1;

    /* compute bytes per pixel (must be whole bytes for this simple copy) */
    int bits_per_pixel = image->bits_per_pixel ? image->bits_per_pixel : (image->depth ? image->depth : 32);
    int bytes_per_pixel = (bits_per_pixel + 7) / 8;
    size_t dst_bpl = (size_t)image->bytes_per_line; /* default destination line stride */
    size_t needed = dst_bpl * (size_t)height;

    if (needed == 0 || needed > IPC_MAX_IMAGE_BYTES) {
        fprintf(stderr, "[ipc-client] XPutImage: image too large (%zu bytes)\n", needed);
        return 1;
    }

    pthread_mutex_lock(&putimage_lock);

    /* Copy rectangle rows into shared region buffer.
       If the source already exactly matches requested rectangle, do a single memcpy. */
    if (src_x == 0 && src_y == 0 && width == (unsigned)image->width && height == (unsigned)image->height
        && (size_t)image->bytes_per_line == dst_bpl) {
        memcpy(g->image_buf, image->data, needed);
    } else {
        /* copy row by row */
        uint8_t *dst = g->image_buf;
        uint8_t *src_base = (uint8_t*)image->data;
        for (unsigned int row = 0; row < height; ++row) {
            size_t src_offset = ((size_t)(src_y + (int)row) * (size_t)image->bytes_per_line) + (size_t)src_x * (size_t)bytes_per_pixel;
            memcpy(dst + (size_t)row * dst_bpl, src_base + src_offset, dst_bpl);
        }
    }

    /* publish size */
    g->image_size = (uint32_t)needed;

    /* prepare RPC */
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XPutImage;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.put_image.drawable = (uint64_t)(uintptr_t)d;
    req.p.put_image.gc = (uint64_t)(uintptr_t)gc;
    req.p.put_image.src_x = src_x;
    req.p.put_image.src_y = src_y;
    req.p.put_image.dst_x = dst_x;
    req.p.put_image.dst_y = dst_y;
    req.p.put_image.width = width;
    req.p.put_image.height = height;
    req.p.put_image.depth = image->depth;
    req.p.put_image.format = image->format;
    req.p.put_image.bytes_per_line = (int)dst_bpl;

    ipc_slot_t resp;
    int rc = 0;
    if (send_request_and_wait(&req, &resp) != 0 || resp.status != 0) {
        fprintf(stderr, "[ipc-client] XPutImage: server RPC failed (status=%d)\n", resp.status);
        rc = 1;
    }

    pthread_mutex_unlock(&putimage_lock);
    return rc;
}

/* Unsupported X11 calls: log and exit */
int XSync(Display *display, Bool discard) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return None;
}

int XChangeProperty(Display *display, Window w, Atom property, Atom type,
                    int format, int mode, const unsigned char *data, int nelements) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

Status XSetWMProtocols(Display *display, Window w, Atom *protocols, int count) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XStoreName(Display *display, Window w, const char *window_name) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XSetWMHints(Display *display, Window w, XWMHints *wmhints) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

XWMHints *XAllocWMHints(void) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XFree(void *data) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XSetStandardProperties(Display *display, Window w, const char *window_name,
                          const char *icon_name, Pixmap icon_pixmap,
                          char **argv, int argc, XSizeHints *hints) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer,
                  KeySym *keysym_return, XComposeStatus *status_in_out) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

int XEventsQueued(Display *display, int mode) {
    /* Same as XPending for our purposes */
    return event_count();
}

int XConnectionNumber(Display *display) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return -1;
}

Status XGetGeometry(Display *display, Drawable d, Window *root_return,
                   int *x_return, int *y_return, unsigned int *width_return,
                   unsigned int *height_return, unsigned int *border_width_return,
                   unsigned int *depth_return) {
    UNSUPPORTED_CALL(); /* Mark as unsupported for now */
    return 0;
}

/* Create a server-side GC and return an opaque GC handle to the client */
GC XCreateGC(Display *display, Drawable d, unsigned long valuemask, XGCValues *values) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XCreateGC;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.create_gc.drawable = (uint64_t)(uintptr_t)d;
    req.p.create_gc.valuemask = valuemask;

    if (values) {
        if (valuemask & GCFunction)          req.p.create_gc.function = (unsigned long)values->function;
        if (valuemask & GCPlaneMask)         req.p.create_gc.plane_mask = values->plane_mask;
        if (valuemask & GCForeground)        req.p.create_gc.foreground = values->foreground;
        if (valuemask & GCBackground)        req.p.create_gc.background = values->background;
        if (valuemask & GCLineWidth)         req.p.create_gc.line_width = values->line_width;
        if (valuemask & GCLineStyle)         req.p.create_gc.line_style = values->line_style;
        if (valuemask & GCCapStyle)          req.p.create_gc.cap_style = values->cap_style;
        if (valuemask & GCJoinStyle)         req.p.create_gc.join_style = values->join_style;
        if (valuemask & GCFillStyle)         req.p.create_gc.fill_style = values->fill_style;
        if (valuemask & GCFillRule)          req.p.create_gc.fill_rule = values->fill_rule;
        if (valuemask & GCArcMode)           req.p.create_gc.arc_mode = values->arc_mode;
        if (valuemask & GCTile)              req.p.create_gc.tile = (uint64_t)(uintptr_t)values->tile;
        if (valuemask & GCStipple)           req.p.create_gc.stipple = (uint64_t)(uintptr_t)values->stipple;
        if (valuemask & GCTileStipXOrigin)   req.p.create_gc.ts_x_origin = values->ts_x_origin;
        if (valuemask & GCTileStipYOrigin)   req.p.create_gc.ts_y_origin = values->ts_y_origin;
        if (valuemask & GCFont)              req.p.create_gc.font = (uint64_t)(uintptr_t)values->font;
        if (valuemask & GCSubwindowMode)     req.p.create_gc.subwindow_mode = values->subwindow_mode;
        if (valuemask & GCGraphicsExposures) req.p.create_gc.graphics_exposures = values->graphics_exposures ? 1 : 0;
        if (valuemask & GCClipXOrigin)       req.p.create_gc.clip_x_origin = values->clip_x_origin;
        if (valuemask & GCClipYOrigin)       req.p.create_gc.clip_y_origin = values->clip_y_origin;
        if (valuemask & GCClipMask)          req.p.create_gc.clip_mask = (uint64_t)(uintptr_t)values->clip_mask;
        if (valuemask & GCDashOffset)        req.p.create_gc.dash_offset = values->dash_offset;
        if (valuemask & GCDashList)          req.p.create_gc.dashes = (unsigned char)values->dashes;
    }

    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) != 0 || resp.status != 0) {
        return (GC)0;
    }
    return (GC)(uintptr_t)resp.ret_handle;
}

int XFreeGC(Display *display, GC gc) {
    ipc_slot_t req; memset(&req, 0, sizeof(req));
    req.op = OP_XFreeGC;
    req.client_handle = (uint64_t)(uintptr_t)display;
    req.p.free_gc.gc = (uint64_t)(uintptr_t)gc;

    ipc_slot_t resp;
    if (send_request_and_wait(&req, &resp) != 0 || resp.status != 0) {
        return 1;
    }
    return 0;
}
