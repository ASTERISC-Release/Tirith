/* server_shm.c
   Single-client X11 IPC server using one large shared memory region with
   process-shared pthread mutexes and condition variables (no sem_* syscalls).

   Build:
     gcc -g -O0 -Wall -Wextra -o server_shm server_shm.c -lX11 -pthread -lrt
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <inttypes.h>

#ifndef imin
#define imin(a,b) (( (a) < (b) ) ? (a) : (b))
#endif

/* server-side maps */
typedef struct DisplayEntry {
    uint64_t client_handle;
    Display *dpy;
    struct DisplayEntry *next;
} DisplayEntry;

#include "client/common.h"

static shared_region_t *g = NULL;
static DisplayEntry *display_map = NULL;
static pthread_mutex_t maps_lock = PTHREAD_MUTEX_INITIALIZER;

#include <signal.h>
static volatile int server_running = 1;

typedef struct GCEntry {
    uint64_t client_gc;
    GC gc;
    struct GCEntry *next;
} GCEntry;

static GCEntry *gc_map = NULL;

static GC lookup_server_gc(uint64_t client_gc) {
    GC ret = 0;
    pthread_mutex_lock(&maps_lock);
    for (GCEntry *e = gc_map; e; e = e->next) {
        if (e->client_gc == client_gc) { ret = e->gc; break; }
    }
    pthread_mutex_unlock(&maps_lock);
    return ret;
}

static void map_server_gc(uint64_t client_gc, GC gc) {
    GCEntry *e = calloc(1, sizeof(*e));
    e->client_gc = client_gc; e->gc = gc;
    pthread_mutex_lock(&maps_lock);
    e->next = gc_map; gc_map = e;
    pthread_mutex_unlock(&maps_lock);
}

static void unmap_server_gc(uint64_t client_gc) {
    pthread_mutex_lock(&maps_lock);
    GCEntry **pp = &gc_map;
    while (*pp) {
        if ((*pp)->client_gc == client_gc) {
            GCEntry *t = *pp;
            *pp = t->next;
            /* Do not XFreeGC here; caller frees with a valid Display */
            free(t);
            break;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&maps_lock);
}

static Display *lookup_server_display(uint64_t client_handle) {
    Display *ret = NULL;
    pthread_mutex_lock(&maps_lock);
    for (DisplayEntry *e = display_map; e; e = e->next) {
        if (e->client_handle == client_handle) { ret = e->dpy; break; }
    }
    pthread_mutex_unlock(&maps_lock);
    return ret;
}
static void map_server_display(uint64_t client_handle, Display *dpy) {
    DisplayEntry *e = calloc(1, sizeof(*e));
    e->client_handle = client_handle; e->dpy = dpy;
    pthread_mutex_lock(&maps_lock);
    e->next = display_map; display_map = e;
    pthread_mutex_unlock(&maps_lock);
}
static void unmap_server_display(uint64_t client_handle) {
    pthread_mutex_lock(&maps_lock);
    DisplayEntry **pp = &display_map;
    while (*pp) {
        if ((*pp)->client_handle == client_handle) {
            DisplayEntry *t = *pp;
            *pp = t->next;
            XCloseDisplay(t->dpy);
            free(t);
            break;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&maps_lock);
}

/* --- Add small X error handler used only for XGetImage calls --- */
static volatile int g_getimage_xerr = 0;
static int getimage_xerror_handler(Display *dpy, XErrorEvent *ev) {
    (void)dpy;
    g_getimage_xerr = ev->error_code;
    return 0;
}

/* event push */
static int push_event(const XEvent *ev) {
    if (!g) return -1;
    if (pthread_mutex_lock(&g->evt_mtx) != 0) return -1;
    uint32_t next_tail = (g->evt_ring.tail + 1) % g->evt_ring.cap;
    if (next_tail == g->evt_ring.head) {
        /* full */
        pthread_mutex_unlock(&g->evt_mtx);
        return -1;
    }
    g->evt_ring.events[g->evt_ring.tail] = *ev;
    g->evt_ring.tail = next_tail;
    pthread_cond_signal(&g->evt_cond);
    pthread_mutex_unlock(&g->evt_mtx);
    return 0;
}

/* process a single RPC */
static void process_rpc(const ipc_slot_t *req, ipc_slot_t *resp) {
    memset(resp, 0, sizeof(*resp));
    resp->op = req->op; resp->pid = req->pid; resp->seq = req->seq; resp->status = 1;

    switch (req->op) {
    case OP_XOpenDisplay: {
        const char *name = req->p.open_display.name;
        Display *dpy = XOpenDisplay(name[0] ? name : NULL);
        if (!dpy) { resp->status = 1; }
        else {
            map_server_display(req->client_handle, dpy);
            resp->status = 0;
            resp->ret_handle = 1; /* opaque success */

            // /* LOG: server-side display diagnostics */
            // int defscr = DefaultScreen(dpy);
            // fprintf(stderr, "server: XOpenDisplay OK (name='%s') dpy=%p DefaultScreen=%d\n",
            //         name[0] ? name : "(null)", (void*)dpy, defscr);
            //
            // /* enumerate visuals and print a short summary for debugging */
            // XVisualInfo tmpl;
            // memset(&tmpl, 0, sizeof(tmpl));
            // tmpl.screen = defscr;
            // int nvis = 0;
            // XVisualInfo *list = XGetVisualInfo(dpy, VisualScreenMask, &tmpl, &nvis);
            // if (!list || nvis == 0) {
            //     fprintf(stderr, "server: XGetVisualInfo(VisualScreenMask) returned n=%d\n", nvis);
            // } else {
            //     fprintf(stderr, "server: visuals on screen %d (n=%d):\n", defscr, nvis);
            //     for (int i = 0; i < nvis; ++i) {
            //         fprintf(stderr, "  [%d] visualid=0x%lx depth=%d class=%d red=0x%lx green=0x%lx blue=0x%lx\n",
            //                 i,
            //                 (unsigned long)list[i].visualid,
            //                 list[i].depth,
            //                 list[i].class,
            //                 (unsigned long)list[i].red_mask,
            //                 (unsigned long)list[i].green_mask,
            //                 (unsigned long)list[i].blue_mask);
            //     }
            //     XFree(list);
            // }
        }
        break;
    }
    case OP_XCloseDisplay: {
        Display *d = lookup_server_display(req->client_handle);
        if (!d) { resp->status = 1; break; }
        unmap_server_display(req->client_handle);
        resp->status = 0;
        break;
    }
    case OP_XCreateSimpleWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window root = DefaultRootWindow(dpy);
        Window w = XCreateSimpleWindow(dpy, root,
                                       req->p.create_window_simple.x, req->p.create_window_simple.y,
                                       req->p.create_window_simple.w, req->p.create_window_simple.h,
                                       req->p.create_window_simple.border,
                                       req->p.create_window_simple.border_px,
                                       req->p.create_window_simple.bg);
        resp->status = 0;
        resp->ret_handle = (uint64_t)w;
        break;
    }
    case OP_XCreateWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window parent = (Window)req->p.create_window.parent;
        XSetWindowAttributes swa;
        memset(&swa, 0, sizeof(swa));
        unsigned long valuemask = 0;

        if (req->p.create_window.valuemask & CWBackPixel) {
            swa.background_pixel = req->p.create_window.background_pixel;
            valuemask |= CWBackPixel;
        }
        if (req->p.create_window.valuemask & CWBorderPixel) {
            swa.border_pixel = req->p.create_window.border_pixel;
            valuemask |= CWBorderPixel;
        }
        if (req->p.create_window.valuemask & CWEventMask) {
            swa.event_mask = req->p.create_window.event_mask;
            valuemask |= CWEventMask;
        }

        /* visual pointer: cast back to Visual* if non-zero. */
        Visual *visual = NULL;
        if (req->p.create_window.visual != 0) visual = (Visual *)(uintptr_t)req->p.create_window.visual;

        Window w = XCreateWindow(dpy, parent,
                                 req->p.create_window.x, req->p.create_window.y,
                                 req->p.create_window.width, req->p.create_window.height,
                                 req->p.create_window.border_width,
                                 req->p.create_window.depth,
                                 req->p.create_window.class,
                                 visual,
                                 valuemask,
                                 &swa);
        resp->status = 0;
        resp->ret_handle = (uint64_t)w;
        break;
    }
    case OP_XMapWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.simple_window.window_handle;
        XMapWindow(dpy, w);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XUnmapWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.unmap_window.window_handle;
        XUnmapWindow(dpy, w);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XConfigureWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.configure_window.window_handle;
        unsigned long mask = req->p.configure_window.value_mask;
        XWindowChanges wc;
        memset(&wc, 0, sizeof(wc));
        if (mask & CWX) wc.x = req->p.configure_window.x;
        if (mask & CWY) wc.y = req->p.configure_window.y;
        if (mask & CWWidth) wc.width = req->p.configure_window.width;
        if (mask & CWHeight) wc.height = req->p.configure_window.height;
        if (mask & CWBorderWidth) wc.border_width = req->p.configure_window.border_width;
        if (mask & CWSibling) wc.sibling = (Window)req->p.configure_window.sibling;
        if (mask & CWStackMode) wc.stack_mode = req->p.configure_window.stack_mode;

        XConfigureWindow(dpy, w, mask, &wc);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XDestroyWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.simple_window.window_handle;
        XDestroyWindow(dpy, w);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XFlush: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XSelectInput: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        Window w = (Window)req->p.select_input.window_handle;
        long mask = (long)req->p.select_input.event_mask;
        XSelectInput(dpy, w, mask);
        XFlush(dpy);
        resp->status = 0;
        break;
    }
    case OP_XDefaultRootWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        /* default root for this display = RootWindow(dpy, DefaultScreen(dpy)) */
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
        resp->status = 0;
        resp->ret_handle = (uint64_t)root;
        break;
    }
    case OP_XRootWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        int screen = req->p.root_window.screen;
        /* sanity: if negative, use DefaultScreen */
        if (screen < 0) screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
        resp->status = 0;
        resp->ret_handle = (uint64_t)root;
        break;
    }
    case OP_XMatchVisualInfo: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; resp->ret_int = 0; break; }

        int screen = req->p.match_visual.screen;
        int depth = req->p.match_visual.depth;
        int vclass = (int)req->p.match_visual.c_class;

        XVisualInfo vinfo_storage;
        XVisualInfo *vinfo = &vinfo_storage;

        /* XMatchVisualInfo returns nonzero on success */
        int matched = XMatchVisualInfo(dpy, screen, depth, vclass, vinfo);

        resp->status = 0;            /* RPC succeeded (we're returning result) */
        resp->ret_int = matched;     /* 1 => matched, 0 => not matched */
        resp->p.match_visual.matched = matched ? 1 : 0;

        if (matched) {
            /* copy relevant fields back to response */
            resp->p.match_visual.visual = (uint64_t)(uintptr_t)vinfo->visual; /* opaque */
            resp->p.match_visual.visualid = vinfo->visualid;
            resp->p.match_visual.depth_ret = vinfo->depth;
            // resp->p.match_visual.c_class_ret = vinfo->c_class; /* if you used c_class name */
            resp->p.match_visual.c_class_ret = vinfo->class; /* if you used c_class name */
            resp->p.match_visual.red_mask = vinfo->red_mask;
            resp->p.match_visual.green_mask = vinfo->green_mask;
            resp->p.match_visual.blue_mask = vinfo->blue_mask;
            resp->p.match_visual.colormap_size = vinfo->colormap_size;
            resp->p.match_visual.bits_per_rgb = vinfo->bits_per_rgb;
        } else {
            /* zero-out the fields */
            resp->p.match_visual.visual = 0;
            resp->p.match_visual.visualid = 0;
            resp->p.match_visual.depth_ret = 0;
            resp->p.match_visual.c_class_ret = 0;
            resp->p.match_visual.red_mask = resp->p.match_visual.green_mask = resp->p.match_visual.blue_mask = 0;
            resp->p.match_visual.colormap_size = 0;
            resp->p.match_visual.bits_per_rgb = 0;
        }
        break;
    }
    case OP_XGetVisualInfo: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; resp->ret_int = 0; break; }

        long mask = req->p.get_visuals.vinfo_mask;

        /* populate a template XVisualInfo from the incoming template fields */
        XVisualInfo tmpl;
        memset(&tmpl, 0, sizeof(tmpl));
        int vmask = 0;
        if (mask & VisualScreenMask) { tmpl.screen = req->p.get_visuals.screen; vmask |= VisualScreenMask; }
        if (mask & VisualDepthMask)  { tmpl.depth  = req->p.get_visuals.depth;  vmask |= VisualDepthMask; }
        // if (mask & VisualClassMask)  { tmpl.c_class = req->p.get_visuals.c_class; vmask |= VisualClassMask; }
        if (mask & VisualClassMask)  { tmpl.class = req->p.get_visuals.c_class; vmask |= VisualClassMask; }
        if (mask & VisualIDMask)     { tmpl.visualid = (VisualID)req->p.get_visuals.visualid; vmask |= VisualIDMask; }
        if (mask & VisualRedMask)    { tmpl.red_mask = req->p.get_visuals.red_mask; vmask |= VisualRedMask; }
        if (mask & VisualGreenMask)  { tmpl.green_mask = req->p.get_visuals.green_mask; vmask |= VisualGreenMask; }
        if (mask & VisualBlueMask)   { tmpl.blue_mask = req->p.get_visuals.blue_mask; vmask |= VisualBlueMask; }
        if (mask & VisualColormapSizeMask) { tmpl.colormap_size = req->p.get_visuals.colormap_size; vmask |= VisualColormapSizeMask; }
        if (mask & VisualBitsPerRGBMask)   { tmpl.bits_per_rgb = req->p.get_visuals.bits_per_rgb; vmask |= VisualBitsPerRGBMask; }

        XVisualInfo *list = NULL;
        int n = 0;

        /* Call the real XGetVisualInfo on the server display */
        list = XGetVisualInfo(dpy, vmask, &tmpl, &n);
        if (!list || n <= 0) {
            resp->status = 0; /* RPC success, but no visuals found */
            resp->ret_int = 0;
            resp->p.get_visuals.nitems = 0;
            if (list) XFree(list);
            break;
        }

        /* copy up to IPC_MAX_VISUALS entries */
        int copied = (n > IPC_MAX_VISUALS) ? IPC_MAX_VISUALS : n;
        for (int i = 0; i < copied; ++i) {
            resp->p.get_visuals.list[i].visualid = list[i].visualid;
            resp->p.get_visuals.list[i].screen = list[i].screen;
            resp->p.get_visuals.list[i].depth = list[i].depth;
            resp->p.get_visuals.list[i].c_class = list[i].class;
            //resp->p.get_visuals.list[i].c_class = list[i].c_class;
            resp->p.get_visuals.list[i].red_mask = list[i].red_mask;
            resp->p.get_visuals.list[i].green_mask = list[i].green_mask;
            resp->p.get_visuals.list[i].blue_mask = list[i].blue_mask;
            resp->p.get_visuals.list[i].colormap_size = list[i].colormap_size;
            resp->p.get_visuals.list[i].bits_per_rgb = list[i].bits_per_rgb;
        }

        resp->status = 0; /* RPC OK */
        resp->ret_int = copied; /* number returned (may be truncated) */
        resp->p.get_visuals.nitems = copied;

        XFree(list);
        break;
    }
    case OP_XDefaultVisual: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; resp->ret_int = 0; break; }

        int screen = req->p.default_visual.screen;

        /* Get the server's DefaultVisual */
        Visual *v = DefaultVisual(dpy, screen);
        if (!v) {
            resp->status = 0;
            resp->ret_int = 0;
            resp->p.default_visual.visualid = 0;
            break;
        }

        VisualID vid = XVisualIDFromVisual(v);
        resp->p.default_visual.visualid = (uint64_t)(uintptr_t)vid;
        resp->ret_handle = (uint64_t)vid;

        /* Try to fetch canonical XVisualInfo for this visualid */
        XVisualInfo tmpl;
        memset(&tmpl, 0, sizeof(tmpl));
        tmpl.visualid = vid;
        int n = 0;
        XVisualInfo *list = XGetVisualInfo(dpy, VisualIDMask, &tmpl, &n);
        if (!list || n == 0) {
            /* best-effort: fill depth & class from DefaultDepth/DefaultVisualClass where possible */
            resp->p.default_visual.depth = DefaultDepth(dpy, screen);
            resp->p.default_visual.c_class = list ? list->class : CopyFromParent; /* fallback */
            resp->p.default_visual.red_mask = resp->p.default_visual.green_mask = resp->p.default_visual.blue_mask = 0;
            resp->p.default_visual.colormap_size = 0;
            resp->p.default_visual.bits_per_rgb = 0;
            if (list) XFree(list);
        } else {
            /* copy fields from found visual */
            resp->p.default_visual.depth = list[0].depth;
            resp->p.default_visual.c_class = list[0].class;
            resp->p.default_visual.red_mask = list[0].red_mask;
            resp->p.default_visual.green_mask = list[0].green_mask;
            resp->p.default_visual.blue_mask = list[0].blue_mask;
            resp->p.default_visual.colormap_size = list[0].colormap_size;
            resp->p.default_visual.bits_per_rgb = list[0].bits_per_rgb;
            XFree(list);
        }

        resp->status = 0;
        resp->ret_int = 1; /* success */
        break;
    }
    case OP_XGetRootWindow: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }
        int screen = req->p.get_root.screen;
        Window root = DefaultRootWindow(dpy); /* or RootWindow(dpy, screen) */
        resp->status = 0;
        resp->p.get_root.window = (uint64_t)(uintptr_t)root;
        resp->ret_handle = (uint64_t)(uintptr_t)root; /* optional convenience */
        break;
    }
    case OP_XPutImage: {
        /* ZERO-COPY: point XImage at shared_region_t.image_buf (client must populate it
           and wait for the RPC to complete before reusing). Validate size first. */
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy || !g) { resp->status = 1; break; }

        uint32_t img_sz = g->image_size;
        int width = (int)req->p.put_image.width;
        int height = (int)req->p.put_image.height;
        int depth = req->p.put_image.depth;
        int format = req->p.put_image.format; /* e.g. ZPixmap */
        int bpl = req->p.put_image.bytes_per_line;

        if (width <= 0 || height <= 0 || bpl <= 0) { resp->status = 1; break; }
        size_t expected = (size_t)bpl * (size_t)height;
        if (img_sz == 0 || img_sz > IPC_MAX_IMAGE_BYTES || img_sz < expected) {
            /* client didn't publish the expected bytes */
            resp->status = 1;
            break;
        }

        /* Create XImage that points directly into the shared region buffer.
           Do NOT free or modify g->image_buf while the server is processing;
           send_request_and_wait ensures client waits for the response. */
        Visual *visual = DefaultVisual(dpy, DefaultScreen(dpy));
        XImage *xim = XCreateImage(dpy, visual, depth, format, 0, (char*)g->image_buf,
                                   (unsigned)width, (unsigned)height, 32, bpl);
        if (!xim) { resp->status = 1; break; }

        Window dst = (Window)(uintptr_t)req->p.put_image.drawable;

        GC gc = DefaultGC(dpy, DefaultScreen(dpy));
        if (req->p.put_image.gc) {
            GC mapped = lookup_server_gc(req->p.put_image.gc);
            if (mapped) gc = mapped;
        }

        XPutImage(dpy, dst, gc, xim,
                  req->p.put_image.src_x, req->p.put_image.src_y,
                  req->p.put_image.dst_x, req->p.put_image.dst_y,
                  (unsigned)width, (unsigned)height);
        XFlush(dpy);

        /* Ensure XDestroyImage will not attempt to free shared-region memory */
        xim->data = NULL;
        XDestroyImage(xim);

        resp->status = 0;
        break;
    }
    case OP_XCreateGC: {
        Display *dpy = lookup_server_display(req->client_handle);
        if (!dpy) { resp->status = 1; break; }

        Drawable dr = (Drawable)(uintptr_t)req->p.create_gc.drawable;
        unsigned long mask = req->p.create_gc.valuemask;
        XGCValues gcv; memset(&gcv, 0, sizeof(gcv));

        if (mask & GCFunction)          gcv.function = (int)req->p.create_gc.function;
        if (mask & GCPlaneMask)         gcv.plane_mask = req->p.create_gc.plane_mask;
        if (mask & GCForeground)        gcv.foreground = req->p.create_gc.foreground;
        if (mask & GCBackground)        gcv.background = req->p.create_gc.background;
        if (mask & GCLineWidth)         gcv.line_width = req->p.create_gc.line_width;
        if (mask & GCLineStyle)         gcv.line_style = req->p.create_gc.line_style;
        if (mask & GCCapStyle)          gcv.cap_style = req->p.create_gc.cap_style;
        if (mask & GCJoinStyle)         gcv.join_style = req->p.create_gc.join_style;
        if (mask & GCFillStyle)         gcv.fill_style = req->p.create_gc.fill_style;
        if (mask & GCFillRule)          gcv.fill_rule = req->p.create_gc.fill_rule;
        if (mask & GCArcMode)           gcv.arc_mode = req->p.create_gc.arc_mode;
        if (mask & GCTile)              gcv.tile = (Pixmap)(uintptr_t)req->p.create_gc.tile;
        if (mask & GCStipple)           gcv.stipple = (Pixmap)(uintptr_t)req->p.create_gc.stipple;
        if (mask & GCTileStipXOrigin)   gcv.ts_x_origin = req->p.create_gc.ts_x_origin;
        if (mask & GCTileStipYOrigin)   gcv.ts_y_origin = req->p.create_gc.ts_y_origin;
        if (mask & GCFont)              gcv.font = (Font)(uintptr_t)req->p.create_gc.font;
        if (mask & GCSubwindowMode)     gcv.subwindow_mode = req->p.create_gc.subwindow_mode;
        if (mask & GCGraphicsExposures) gcv.graphics_exposures = req->p.create_gc.graphics_exposures ? True : False;
        if (mask & GCClipXOrigin)       gcv.clip_x_origin = req->p.create_gc.clip_x_origin;
        if (mask & GCClipYOrigin)       gcv.clip_y_origin = req->p.create_gc.clip_y_origin;
        if (mask & GCClipMask)          gcv.clip_mask = (Pixmap)(uintptr_t)req->p.create_gc.clip_mask;
        if (mask & GCDashOffset)        gcv.dash_offset = req->p.create_gc.dash_offset;
        if (mask & GCDashList)          gcv.dashes = (char)req->p.create_gc.dashes;

        GC gc = XCreateGC(dpy, dr, mask, &gcv);
        if (!gc) { resp->status = 1; break; }

        /* Use the server GC pointer value as the opaque handle */
        uint64_t handle = (uint64_t)(uintptr_t)gc;
        map_server_gc(handle, gc);
        resp->status = 0;
        resp->ret_handle = handle;
        break;
    }
    case OP_XFreeGC: {
        Display *dpy = lookup_server_display(req->client_handle);
        uint64_t h = req->p.free_gc.gc;
        GC gc = lookup_server_gc(h);
        if (!dpy || !gc) { resp->status = 1; break; }
        XFreeGC(dpy, gc);     /* free once with a valid Display */
        unmap_server_gc(h);   /* just unlink the entry */
        resp->status = 0;
        break;
    }
    default:
        resp->status = 1;
        break;
    }
}

/* pull events from server displays and push into shared ring */
static void dispatch_events_all(void) {
    pthread_mutex_lock(&maps_lock);
    for (DisplayEntry *e = display_map; e; e = e->next) {
        Display *dpy = e->dpy;
        // Process ALL pending events for this display
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            ev.xany.display = NULL; /* client will set local display pointer */
            if (push_event(&ev) != 0) {
                fprintf(stderr, "server_shm: event ring full; dropping event\n");
            } else {
                fprintf(stderr, "Pushed an event type=%d\n", ev.type);
            }
        }
    }
    pthread_mutex_unlock(&maps_lock);
}

static void* event_dispatcher_thread(void *arg) {
    (void)arg;
    while (server_running) {
        dispatch_events_all();
        usleep(1000); // 1ms sleep to avoid busy waiting
    }
    return NULL;
}

static void cleanup_displays(void) {
    pthread_mutex_lock(&maps_lock);
    DisplayEntry *current = display_map;
    while (current) {
        DisplayEntry *next = current->next;
        if (current->dpy) {
            XCloseDisplay(current->dpy);
        }
        free(current);
        current = next;
    }
    display_map = NULL;
    pthread_mutex_unlock(&maps_lock);
}

// Add signal handler for clean shutdown
static void signal_handler(int sig) {
    (void)sig;
    server_running = 0;
}

unsigned long roundToNearestMB(unsigned long bytes) {
    const unsigned long MB = 1024 * 1024;
    // Add half MB and then truncate to get rounding effect
    return ((bytes + MB/2) / MB) + 5 * MB;
}

int main(void) {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) { perror("shm_open"); return 1; }
    if (ftruncate(fd, roundToNearestMB(sizeof(shared_region_t))) != 0) { perror("ftruncate"); return 1; }
    void *p = mmap(NULL, roundToNearestMB(sizeof(shared_region_t)), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    g = (shared_region_t*)p;

    fprintf(stderr, "server_shm: pid=%d shared=%s ptr=%p\n", (int)getpid(), SHM_NAME, (void*)g);
    fprintf(stderr, "server_shm: sizeof(shared_region_t)=%zu\n", roundToNearestMB(sizeof(shared_region_t)));

    /* Initialize shared region: set mutex/cond attributes (only server does this) */
    pthread_mutexattr_t mattr;
    pthread_condattr_t  cattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
#ifdef __linux__
    /* robust can be helpful but requires careful handling on owner death; optional */
    // pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);
#endif
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    /* initialize mutexes/conds and flags */
    pthread_mutex_init(&g->req_mtx, &mattr);
    pthread_cond_init(&g->req_cond, &cattr);
    g->req_ready = 0;
    g->resp_ready = 0;
    memset(&g->rpc_slot, 0, sizeof(g->rpc_slot));

    pthread_mutex_init(&g->evt_mtx, &mattr);
    pthread_cond_init(&g->evt_cond, &cattr);
    g->evt_ring.head = g->evt_ring.tail = 0;
    g->evt_ring.cap = EVENT_RING_CAP;

    // Set up signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Start event dispatcher thread
    pthread_t evt_thread;
    if (pthread_create(&evt_thread, NULL, event_dispatcher_thread, NULL) != 0) {
        perror("pthread_create event dispatcher");
        return 1;
    }

    fprintf(stderr, "server_shm: ready (shared=%s)\n", SHM_NAME);

    /* main loop */
    while (server_running) {
        /* wait for request */
        if (pthread_mutex_lock(&g->req_mtx) != 0) { 
            perror("pthread_mutex_lock req_mtx"); 
            break; 
        }
        while (!g->req_ready && server_running) {
            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 1; // 1 second timeout
            if (pthread_cond_timedwait(&g->req_cond, &g->req_mtx, &timeout) == ETIMEDOUT) {
                if (!server_running) break;
            }
        }
        
        if (!server_running) {
            pthread_mutex_unlock(&g->req_mtx);
            break;
        }
        
        /* copy request */
        ipc_slot_t req;
        memcpy(&req, &g->rpc_slot, sizeof(req));

        /* reset request flag */
        g->req_ready = 0;
        pthread_mutex_unlock(&g->req_mtx);
        
        fprintf(stderr, "server_shm: got request pid=%d op=%u seq=%u client_handle=0x%016" PRIx64 "\n",
                (int)req.pid, (unsigned)req.op, (unsigned)req.seq, req.client_handle);
        
        /* process it */
        ipc_slot_t resp;
        process_rpc(&req, &resp);
        
        /* write response and signal client */
        if (pthread_mutex_lock(&g->req_mtx) != 0) { 
            perror("pthread_mutex_lock resp"); 
            break; 
        }
        memcpy(&g->rpc_slot, &resp, sizeof(resp));
        g->resp_ready = 1;
        pthread_cond_signal(&g->req_cond);
        pthread_mutex_unlock(&g->req_mtx);
    }

    fprintf(stderr, "server_shm: shutting down...\n");
    server_running = 0;
    pthread_join(evt_thread, NULL);

    cleanup_displays();
    shm_unlink(SHM_NAME);

    return 0;
}

