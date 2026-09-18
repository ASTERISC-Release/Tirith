#ifndef __COMMON_H__
#define __COMMON_H__

#define SHM_NAME "/x11ipc_shm_all"
#define EVENT_RING_CAP 256

enum {
    OP_NOP = 0,
    OP_XOpenDisplay = 1,
    OP_XCloseDisplay,
    OP_XCreateSimpleWindow,
    OP_XCreateWindow,
    OP_XMapWindow,
    OP_XUnmapWindow,
    OP_XConfigureWindow,
    OP_XDestroyWindow,
    OP_XFlush,
    OP_XSelectInput,
    OP_XDefaultRootWindow,
    OP_XRootWindow,
    OP_XMatchVisualInfo,
    OP_XGetVisualInfo,
    OP_XDefaultVisual,
    OP_XGetRootWindow,
    OP_XPutImage,
    OP_XCreateGC,      /* NEW */
    OP_XFreeGC,        /* NEW */
};

#define IPC_MAX_VISUALS 300
#define IPC_MAX_IMAGE_BYTES (2 * 1024 * 1024)

typedef struct {
    uint32_t op;
    pid_t pid;
    uint32_t seq;
    uint64_t client_handle;
    union {
        struct { char name[240]; } open_display;
        struct { uint64_t handle; } close_display;
        /* create_simple kept for backward compatibility */
        struct { int x,y,w,h,border; unsigned long border_px, bg; } create_window_simple;
        /* full create window */
        struct {
            uint64_t parent;         /* Window parent (opaque numeric handle) */
            int x, y;
            unsigned int width, height;
            unsigned int border_width;
            int depth;
            unsigned int class;      /* InputOutput, InputOnly, etc. */
            uint64_t visual;         /* Visual* as opaque uint64 (may be NULL/0) */
            unsigned long valuemask; /* valuemask bits (CWBackPixel, CWBorderPixel, CWEventMask, ...) */
            unsigned long background_pixel;
            unsigned long border_pixel;
            unsigned long event_mask;
        } create_window;

        /* XGetImage request/response */
        struct {
            uint64_t drawable;      /* request: drawable */
            int x, y;               /* request: origin */
            unsigned int width, height; /* request */
            unsigned long plane_mask; /* request: plane mask */
            int format;             /* request: ZPixmap/XYBitmap/etc */
            /* response fields filled by server */
            int depth;              /* response: depth of returned image */
            int bytes_per_line;     /* response: bytes per scanline */
        } get_image;
        
        struct { uint64_t window_handle; unsigned long event_mask; } select_input;
        struct { uint64_t window_handle; } simple_window;
        /* unmap */
        struct { uint64_t window_handle; } unmap_window;
        /* configure */
        struct {
            uint64_t window_handle;
            unsigned long value_mask;
            int x, y;
            unsigned int width, height;
            unsigned int border_width;
            uint64_t sibling;
            int stack_mode;
        } configure_window;

        /* root window */
        struct {
            int screen;
        } root_window;

        /* XMatchVisualInfo request/response */
        struct {
            int screen;            /* request: screen number */
            int depth;             /* request: depth to match */
            unsigned int c_class;  /* request: visual class (PseudoColor, TrueColor, etc.) */
            /* response fields (filled by server) */
            uint64_t visual;       /* opaque Visual* as uint64 (CLIENT: will probably be NULL) */
            VisualID visualid;     /* VisualID returned by server */
            int depth_ret;         /* returned depth */
            int c_class_ret;       /* returned class (store in c_class_ret) */
            unsigned long red_mask;
            unsigned long green_mask;
            unsigned long blue_mask;
            int colormap_size;
            int bits_per_rgb;
            int matched;           /* 1 if match found, 0 otherwise */
        } match_visual;

        struct {
            long vinfo_mask;                /* request: mask (VisualScreenMask etc) */
            /* template fields client provided (only the fields corresponding
               to bits set in vinfo_mask should be considered) */
            int screen;                     /* template: screen */
            int depth;                      /* template: depth */
            int c_class;                    /* template: class */
            VisualID visualid;              /* template: visualid */
            unsigned long red_mask;         /* template: red_mask */
            unsigned long green_mask;       /* template: green_mask */
            unsigned long blue_mask;        /* template: blue_mask */
            int colormap_size;              /* template */
            int bits_per_rgb;               /* template */

            /* response: number of visuals returned (nitems) */
            int nitems;

            /* array of returned visuals (serialized) */
            struct {
                VisualID visualid;
                int screen;
                int depth;
                int c_class;
                unsigned long red_mask;
                unsigned long green_mask;
                unsigned long blue_mask;
                int colormap_size;
                int bits_per_rgb;
            } list[IPC_MAX_VISUALS];
        } get_visuals;

        struct {
            int screen;                /* request: screen index */
            uint64_t visualid;         /* response: VisualID of default visual */
            int depth;                 /* response */
            int c_class;               /* response (class) */
            unsigned long red_mask;    /* response */
            unsigned long green_mask;  /* response */
            unsigned long blue_mask;   /* response */
            int colormap_size;         /* response */
            int bits_per_rgb;          /* response */
        } default_visual;

        struct {
            int screen;          /* request: screen index */
            uint64_t window;     /* response: server-side Window id */
        } get_root;

        struct {
            uint64_t drawable;      /* destination Drawable/Window */
            uint64_t gc;            /* GC (opaque numeric handle) or 0 for default */
            int src_x, src_y;       /* source offsets inside the image */
            int dst_x, dst_y;       /* dest offsets on the drawable */
            unsigned int width, height;
            int depth;              /* image depth */
            int format;             /* ZPixmap, XYBitmap, etc. (use Xlib constants) */
            int bytes_per_line;     /* bytes per scanline in image_buf */
        } put_image;

        /* NEW: XCreateGC payload (serialize commonly used XGCValues fields) */
        struct {
            uint64_t drawable;
            unsigned long valuemask;

            unsigned long function;
            unsigned long plane_mask;
            unsigned long foreground;
            unsigned long background;
            int           line_width;
            int           line_style;
            int           cap_style;
            int           join_style;
            int           fill_style;
            int           fill_rule;
            int           arc_mode;

            uint64_t      tile;            /* Pixmap */
            uint64_t      stipple;         /* Pixmap */
            int           ts_x_origin;
            int           ts_y_origin;

            uint64_t      font;            /* Font */
            int           subwindow_mode;
            int           graphics_exposures; /* Bool, send as int */
            int           clip_x_origin;
            int           clip_y_origin;
            uint64_t      clip_mask;       /* Pixmap or None */

            int           dash_offset;
            unsigned char dashes;
        } create_gc;

        /* NEW: XFreeGC payload */
        struct {
            uint64_t gc;
        } free_gc;
    } p;
    int32_t ret_int;
    uint64_t ret_handle;
    int8_t status;
} ipc_slot_t;

typedef struct {
    uint32_t head;
    uint32_t tail;
    uint32_t cap;
    XEvent events[EVENT_RING_CAP];
} event_ring_t;

typedef struct {
    pthread_mutex_t req_mtx;
    pthread_cond_t  req_cond;
    int req_ready;
    int resp_ready;
    ipc_slot_t rpc_slot;
    uint32_t image_size; /* actual bytes in image_buf */
    uint8_t  image_buf[IPC_MAX_IMAGE_BYTES];
    pthread_mutex_t evt_mtx;
    pthread_cond_t  evt_cond;
    event_ring_t evt_ring;
} shared_region_t;

#define NUM_DISPLAYS 16

/* events.c */
int send_request_and_wait(ipc_slot_t *req, ipc_slot_t *resp_out);
int event_count(void);
int pop_event(XEvent *out);

/* Visual mask constants — add to common.h or Xutil.h if missing */
#ifndef VisualScreenMask
#define VisualScreenMask           (1L << 0)
#endif
#ifndef VisualDepthMask
#define VisualDepthMask            (1L << 1)
#endif
#ifndef VisualClassMask
#define VisualClassMask            (1L << 2)
#endif
#ifndef VisualRedMask
#define VisualRedMask              (1L << 3)
#endif
#ifndef VisualGreenMask
#define VisualGreenMask            (1L << 4)
#endif
#ifndef VisualBlueMask
#define VisualBlueMask             (1L << 5)
#endif
#ifndef VisualColormapSizeMask
#define VisualColormapSizeMask     (1L << 6)
#endif
#ifndef VisualBitsPerRGBMask
#define VisualBitsPerRGBMask       (1L << 7)
#endif
#ifndef VisualIDMask
#define VisualIDMask               (1L << 8)
#endif

#endif
