#ifndef X11_XUTIL_H
#define X11_XUTIL_H

#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Window classes */
#define InputOutput 1
#define InputOnly 2

/* ConfigureWindow value mask bits */
#define CWX (1<<0)
#define CWY (1<<1)
#define CWWidth (1<<2)
#define CWHeight (1<<3)
#define CWBorderWidth (1<<4)
#define CWSibling (1<<5)
#define CWStackMode (1<<6)

/* Value mask bits for XCreateWindow */
#define CWBackPixmap    (1L<<0)
#define CWBackPixel     (1L<<1)
#define CWBorderPixmap  (1L<<2)
#define CWBorderPixel   (1L<<3)
#define CWBitGravity    (1L<<4)
#define CWWinGravity    (1L<<5)
#define CWBackingStore  (1L<<6)
#define CWBackingPlanes (1L<<7)
#define CWBackingPixel  (1L<<8)
#define CWOverrideRedirect (1L<<9)
#define CWSaveUnder     (1L<<10)
#define CWEventMask     (1L<<11)
#define CWDontPropagate (1L<<12)
#define CWColormap      (1L<<13)
#define CWCursor        (1L<<14)

/* Forward declarations */
typedef struct _XDisplay Display;
typedef struct _XGC *GC;
typedef struct _Visual Visual;
typedef struct _Screen Screen;

/* Simplified Display structure */
struct _XDisplay {
    int fd;                    /* File descriptor (fake) */
    int default_screen;        /* Default screen number */
    Screen *screens;           /* Screen array (fake) */
    int nscreens;              /* Number of screens */
    /* Add other fields as needed */
    void *private1;            /* Private data */
    void *private2;            /* Private data */
};

/* Simplified Screen structure */
struct _Screen {
    int width, height;         /* Screen dimensions */
    int mwidth, mheight;       /* Screen dimensions in millimeters */
    Window root;               /* Root window */
    /* Add other fields as needed */
};

/* Simplified Visual structure */
struct _Visual {
    VisualID visualid;
    int class;
    unsigned long red_mask, green_mask, blue_mask;
    int bits_per_rgb;
    int map_entries;
};

/* Additional event structures */
typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    Bool same_screen;
} XKeyEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int button;
    Bool same_screen;
} XButtonEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    int x, y;
    int width, height;
    int count;
} XExposeEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Atom message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
} XClientMessageEvent;

typedef struct {
    long flags;
    Bool input;
    int initial_state;
    Pixmap icon_pixmap;
    Window icon_window;
    int icon_x, icon_y;
    Pixmap icon_mask;
    XID window_group;
} XWMHints;

typedef struct {
    long flags;
    int x, y;
    int width, height;
    int min_width, min_height;
    int max_width, max_height;
    int width_inc, height_inc;
    struct {
        int x;
        int y;
    } min_aspect, max_aspect;
    int base_width, base_height;
    int win_gravity;
} XSizeHints;

typedef struct {
    Pixmap background_pixmap;
    unsigned long background_pixel;
    Pixmap border_pixmap;
    unsigned long border_pixel;
    int bit_gravity;
    int win_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    Bool save_under;
    long event_mask;
    long do_not_propagate_mask;
    Bool override_redirect;
    Colormap colormap;
    Cursor cursor;
} XSetWindowAttributes;

typedef struct {
    int x, y;
    int width, height;
    int border_width;
    Window sibling;
    int stack_mode;
} XWindowChanges;

typedef struct _XComposeStatus {
    void *compose_ptr;
    int chars_matched;
} XComposeStatus;

/* Complete XEvent union */
typedef union _XEvent {
    int type;
    XAnyEvent xany;
    XKeyEvent xkey;
    XButtonEvent xbutton;
    XExposeEvent xexpose;
    XClientMessageEvent xclient;
    long pad[24];
} XEvent;

typedef struct {
    Visual *visual;            /* pointer to Visual structure */
    VisualID visualid;         /* XID for the visual */
    int screen;                /* screen number */
    int depth;                 /* bits per pixel */
    int c_class;               /* visual class (PseudoColor, TrueColor, etc.) */
    unsigned long red_mask;    /* red mask */
    unsigned long green_mask;  /* green mask */
    unsigned long blue_mask;   /* blue mask */
    int colormap_size;         /* colormap size (entries) */
    int bits_per_rgb;          /* number of bits per RGB value */
} XVisualInfo;

typedef struct {
    int type;                 /* event type (ConfigureNotify) */
    unsigned long serial;     /* # of last request processed by server */
    Bool send_event;          /* true if sent explicitly (SendEvent) */
    Display *display;         /* display the event was read from */
    Window event;             /* window the event was reported relative to */
    Window window;            /* the actual window that was configured */
    int x, y;                 /* new position */
    int width, height;        /* new size */
    int border_width;         /* new border width */
    Window above;             /* sibling window above which it was placed (or None) */
    Bool override_redirect;   /* window has override-redirect set? */
} XConfigureEvent;

typedef XID Drawable;
typedef XID Pixmap;
typedef XID Window;
typedef XID Atom;

/* Minimal XImage struct sufficient for XCreateImage/XPutImage usage in the client:
   we keep only fields we read/write in wrappers: width/height/depth/format/data/.. */
typedef struct _XImage {
    int width;
    int height;
    int xoffset;
    int format;
    char *data;
    int byte_order;
    int bitmap_unit;
    int bitmap_bit_order;
    int bitmap_pad;
    int depth;
    int bytes_per_line;
    int bits_per_pixel;
    void *obdata;
    /* omit method pointers (we won't call them from client wrappers) */
} XImage;

/* Now properly declare XNextEvent with XEvent* */
// #undef XNextEvent  /* Remove the forward declaration */
int XNextEvent(Display *display, XEvent *event_return);

/* Extended function declarations */
Window XCreateWindow(Display *display, Window parent, int x, int y,
                     unsigned int width, unsigned int height, unsigned int border_width,
                     int depth, unsigned int class, Visual *visual,
                     unsigned long valuemask, XSetWindowAttributes *attributes);
int XConfigureWindow(Display *display, Window w, unsigned int value_mask, XWindowChanges *changes);
Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists);
int XChangeProperty(Display *display, Window w, Atom property, Atom type,
                   int format, int mode, const unsigned char *data, int nelements);
Status XSetWMProtocols(Display *display, Window w, Atom *protocols, int count);
int XStoreName(Display *display, Window w, const char *window_name);
Status XSetWMHints(Display *display, Window w, XWMHints *wmhints);
XWMHints *XAllocWMHints(void);
int XFree(void *data);
int XSetStandardProperties(Display *display, Window w, const char *window_name,
                          const char *icon_name, Pixmap icon_pixmap,
                          char **argv, int argc, XSizeHints *hints);
int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer,
                  KeySym *keysym_return, XComposeStatus *status_in_out);
int XEventsQueued(Display *display, int mode);
Status XGetGeometry(Display *display, Drawable d, Window *root_return,
                   int *x_return, int *y_return, unsigned int *width_return,
                   unsigned int *height_return, unsigned int *border_width_return,
                   unsigned int *depth_return);
int XMoveWindow(Display *display, Window w, int x, int y);
int XResizeWindow(Display *display, Window w, unsigned int width, unsigned int height);
int XMatchVisualInfo(Display *display, int screen, int depth, int vclass, XVisualInfo *vinfo);
XVisualInfo *XGetVisualInfo(Display *display, long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return);

XImage *XCreateImage(Display *display, Visual *visual, unsigned int depth,
                            int format, int offset, char *data,
                            unsigned int width, unsigned int height,
                            int bitmap_pad, int bytes_per_line);
int XDestroyImage(XImage *image);
int XPutImage(Display *display, Drawable d, GC gc, XImage *image,
              int src_x, int src_y, int dest_x, int dest_y,
              unsigned int width, unsigned int height);
XImage *XGetImage(Display *display, Drawable d, int x, int y, unsigned int width, unsigned int height, unsigned long plane_mask, int format);


/* Add other helper macros */
#define DefaultGC(dpy, screen) ((GC)0)
#define DefaultScreen(d) XDefaultScreen(d)
#define DefaultVisual(dpy, screen) XDefaultVisual((dpy),(screen))
#define DefaultDepth(dpy, scr) 24 // Assume 24 bits per pixel for simplicity (FIX needed)
//#define DefaultRootWindow(d) ((d)->screens[XDefaultScreen(d)].root)
//#define RootWindow(d, scr) ((d)->screens[scr].root)
#define DefaultRootWindow(dpy) XDefaultRootWindow((dpy))
#define RootWindow(dpy, scr) XRootWindow((dpy),(scr))
#define DisplayWidth(d, scr) ((d)->screens[scr].width)
#define DisplayHeight(d, scr) ((d)->screens[scr].height)
#define ScreenOfDisplay(d, scr) (&(d)->screens[scr])

/* Visual info masks */
#define VisualNoMask             0L
#define VisualIDMask             (1L << 0)
#define VisualScreenMask         (1L << 1)
#define VisualDepthMask          (1L << 2)
#define VisualClassMask          (1L << 3)
#define VisualRedMask            (1L << 4)
#define VisualGreenMask          (1L << 5)
#define VisualBlueMask           (1L << 6)
#define VisualColormapSizeMask   (1L << 7)
#define VisualBitsPerRGBMask     (1L << 8)

/* Bitmaps */ 
#define XYBitmap   0
#define XYPixmap   1
#define ZPixmap    2

/* Useful combination masks */
#define VisualAllMask  (VisualIDMask | VisualScreenMask | VisualDepthMask | \
                        VisualClassMask | VisualRedMask | VisualGreenMask |  \
                        VisualBlueMask | VisualColormapSizeMask | VisualBitsPerRGBMask)

/* Pixel helpers */
unsigned long MakePixel(unsigned char red, unsigned char green, unsigned char blue);
#define BlackPixelValue    0x000000UL
#define WhitePixelValue    0xFFFFFFUL
#define RedPixelValue      0xFF0000UL
#define GreenPixelValue    0x00FF00UL
#define BluePixelValue     0x0000FFUL
#define YellowPixelValue   0xFFFF00UL
#define CyanPixelValue     0x00FFFFUL
#define MagentaPixelValue  0xFF00FFUL
#define GrayPixelValue     0x808080UL
#define DarkGrayPixelValue 0x404040UL
#define LightGrayPixelValue 0xC0C0C0UL

/* Key Symbols */ 

#define XK_VoidSymbol      0xffffff  /* Void symbol */
/* Latin-1 & basic ASCII */
#define XK_Escape          0xff1b
#define XK_Tab             0xff09
#define XK_Return          0xff0d
#define XK_BackSpace       0xff08
#define XK_Delete          0xffff
#define XK_Insert          0xff63
#define XK_Home            0xff50
#define XK_End             0xff57
#define XK_Page_Up         0xff55
#define XK_Page_Down       0xff56

/* Arrow keys */
#define XK_Left            0xff51
#define XK_Up              0xff52
#define XK_Right           0xff53
#define XK_Down            0xff54

/* Function keys F1–F12 */
#define XK_F1              0xffbe
#define XK_F2              0xffbf
#define XK_F3              0xffc0
#define XK_F4              0xffc1
#define XK_F5              0xffc2
#define XK_F6              0xffc3
#define XK_F7              0xffc4
#define XK_F8              0xffc5
#define XK_F9              0xffc6
#define XK_F10             0xffc7
#define XK_F11             0xffc8
#define XK_F12             0xffc9

/* Modifier & Misc keys */
#define XK_Shift_L         0xffe1
#define XK_Shift_R         0xffe2
#define XK_Control_L       0xffe3
#define XK_Control_R       0xffe4
#define XK_Alt_L           0xffe9
#define XK_Alt_R           0xffea
#define XK_Meta_L          0xffe7
#define XK_Meta_R          0xffe8
#define XK_Super_L         0xffeb
#define XK_Super_R         0xffec
#define XK_Menu            0xff67
#define XK_Num_Lock        0xff7f
#define XK_Caps_Lock       0xffe5
#define XK_Scroll_Lock     0xff14
#define XK_Pause           0xff13

/* Keypad keys */
#define XK_KP_0            0xffb0
#define XK_KP_1            0xffb1
#define XK_KP_2            0xffb2
#define XK_KP_3            0xffb3
#define XK_KP_4            0xffb4
#define XK_KP_5            0xffb5
#define XK_KP_6            0xffb6
#define XK_KP_7            0xffb7
#define XK_KP_8            0xffb8
#define XK_KP_9            0xffb9
#define XK_KP_Enter        0xff8d
#define XK_KP_Decimal      0xffae
#define XK_KP_Add          0xffab
#define XK_KP_Subtract     0xffad
#define XK_KP_Multiply     0xffaa
#define XK_KP_Divide       0xffaf

/* Letters/digits (these normally map to Unicode codepoints) */
#define XK_q               0x0071
#define XK_w               0x0077
#define XK_e               0x0065
#define XK_r               0x0072
#define XK_t               0x0074
#define XK_y               0x0079
#define XK_u               0x0075
#define XK_i               0x0069
#define XK_o               0x006f
#define XK_p               0x0070

#define XK_a               0x0061
#define XK_s               0x0073
#define XK_d               0x0064
#define XK_f               0x0066
#define XK_g               0x0067
#define XK_h               0x0068
#define XK_j               0x006a
#define XK_k               0x006b
#define XK_l               0x006c

#define XK_z               0x007a
#define XK_x               0x0078
#define XK_c               0x0063
#define XK_v               0x0076
#define XK_b               0x0062
#define XK_n               0x006e
#define XK_m               0x006d

#define XK_0               0x0030
#define XK_1               0x0031
#define XK_2               0x0032
#define XK_3               0x0033
#define XK_4               0x0034
#define XK_5               0x0035
#define XK_6               0x0036
#define XK_7               0x0037
#define XK_8               0x0038
#define XK_9               0x0039

#ifdef __cplusplus
}
#endif

#endif /* X11_XUTIL_H */
