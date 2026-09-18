#ifndef X11_XLIB_H
#define X11_XLIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* Basic types */
typedef unsigned long Atom;
typedef unsigned long VisualID;
typedef unsigned long Time;
typedef unsigned long KeyCode;
typedef unsigned long KeySym;
typedef int Bool;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Font;
typedef XID Pixmap;
typedef XID Cursor;
typedef XID Colormap;
typedef XID GContext;
typedef XID KeySym;
typedef int Status;

#define True 1
#define False 0
#define None 0L
#define CurrentTime 0L

/* Forward declarations */
typedef struct _XDisplay Display;
typedef struct _XGC *GC;
typedef struct _Visual Visual;
typedef struct _Screen Screen;

/* Basic event structure */
typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
} XAnyEvent;

/* Event masks */
#define NoEventMask 0L
#define KeyPressMask (1L<<0)
#define KeyReleaseMask (1L<<1)
#define ButtonPressMask (1L<<2)
#define ButtonReleaseMask (1L<<3)
#define EnterWindowMask (1L<<4)
#define LeaveWindowMask (1L<<5)
#define PointerMotionMask (1L<<6)
#define PointerMotionHintMask (1L<<7)
#define Button1MotionMask (1L<<8)
#define Button2MotionMask (1L<<9)
#define Button3MotionMask (1L<<10)
#define Button4MotionMask (1L<<11)
#define Button5MotionMask (1L<<12)
#define ButtonMotionMask (1L<<13)
#define KeymapStateMask (1L<<14)
#define ExposureMask (1L<<15)
#define VisibilityChangeMask (1L<<16)
#define StructureNotifyMask (1L<<17)
#define ResizeRedirectMask (1L<<18)
#define SubstructureNotifyMask (1L<<19)
#define SubstructureRedirectMask (1L<<20)
#define FocusChangeMask (1L<<21)
#define PropertyChangeMask (1L<<22)
#define ColormapChangeMask (1L<<23)
#define OwnerGrabButtonMask (1L<<24)

/* Event types */
#define KeyPress        2
#define KeyRelease      3
#define ButtonPress     4
#define ButtonRelease   5
#define MotionNotify    6
#define EnterNotify     7
#define LeaveNotify     8
#define FocusIn         9
#define FocusOut        10
#define KeymapNotify    11
#define Expose          12
#define GraphicsExpose  13
#define NoExpose        14
#define VisibilityNotify 15
#define CreateNotify    16
#define DestroyNotify   17
#define UnmapNotify     18
#define MapNotify       19
#define MapRequest      20
#define ReparentNotify  21
#define ConfigureNotify 22
#define ConfigureRequest 23
#define GravityNotify   24
#define ResizeRequest   25
#define CirculateNotify 26
#define CirculateRequest 27
#define PropertyNotify  28
#define SelectionClear  29
#define SelectionRequest 30
#define SelectionNotify 31
#define ColormapNotify  32
#define ClientMessage   33
#define MappingNotify   34
#define GenericEvent    35
#define LASTEvent       36

#define AllPlanes (~0UL)

/* Function declarations */
Display *XOpenDisplay(const char *display_name);
int XCloseDisplay(Display *display);
Window XCreateSimpleWindow(Display *display, Window parent, int x, int y,
                          unsigned int width, unsigned int height,
                          unsigned int border_width, unsigned long border,
                          unsigned long background);
int XMapWindow(Display *display, Window w);
int XUnmapWindow(Display *display, Window w);
int XDestroyWindow(Display *display, Window w);
int XFlush(Display *display);
int XPending(Display *display);
int XSelectInput(Display *display, Window w, long event_mask);
int XDefaultScreen(Display *display);

int XSync(Display *display, Bool discard);
Window XDefaultRootWindow(Display *display);
Window XRootWindow(Display *display, int screen_number);

Visual *XDefaultVisual(Display *display, int screen);

#define StaticGray   0  /* single plane, no color map, gray scale */
#define GrayScale    1  /* gray scale, dynamic colormap */
#define StaticColor  2  /* predefined color map, read-only */
#define PseudoColor  3  /* dynamic colormap, color indices */
#define TrueColor    4  /* RGB directly encoded in pixel value */
#define DirectColor  5  /* separate dynamic maps for R, G, B */

/* Macros */
#define DefaultScreen(d) XDefaultScreen(d)
#define BlackPixel(d, scr) (0x000000UL)
#define WhitePixel(d, scr) (0xFFFFFFUL)

#define DisplayWidthMM(dpy, scr) XDisplayWidthMM(dpy, scr)
#define DisplayHeightMM(dpy, scr) XDisplayHeightMM(dpy, scr)
#define ScreenCount(dpy) XScreenCount(dpy)

#ifndef LSBFirst
/* byte order for image data */
#define LSBFirst 0
#endif

#ifndef MSBFirst
#define MSBFirst 1
#endif

/* X protocol / Xlib return codes and error codes */
#ifndef Success
#define Success 0
#endif

#ifndef BadAlloc
/* X11 BadAlloc is 11 in standard X headers */
#define BadAlloc 11
#endif

/* Other common X error codes (optional, add if you need them) */
#ifndef BadDrawable
#define BadDrawable 9
#endif
#ifndef BadMatch
#define BadMatch 8
#endif
#ifndef BadAccess
#define BadAccess 10
#endif

/* GC valuemask bits (subset of Xlib) */
#define GCFunction            (1L<<0)
#define GCPlaneMask           (1L<<1)
#define GCForeground          (1L<<2)
#define GCBackground          (1L<<3)
#define GCLineWidth           (1L<<4)
#define GCLineStyle           (1L<<5)
#define GCCapStyle            (1L<<6)
#define GCJoinStyle           (1L<<7)
#define GCFillStyle           (1L<<8)
#define GCFillRule            (1L<<9)
#define GCTile                (1L<<10)
#define GCStipple             (1L<<11)
#define GCTileStipXOrigin     (1L<<12)
#define GCTileStipYOrigin     (1L<<13)
#define GCFont                (1L<<14)
#define GCSubwindowMode       (1L<<15)
#define GCGraphicsExposures   (1L<<16)
#define GCClipXOrigin         (1L<<17)
#define GCClipYOrigin         (1L<<18)
#define GCClipMask            (1L<<19)
#define GCDashOffset          (1L<<20)
#define GCDashList            (1L<<21)
#define GCArcMode             (1L<<22)

/* Some common GC attribute constants */
#define LineSolid             0
#define LineOnOffDash         1
#define LineDoubleDash        2

#define CapNotLast            0
#define CapButt               1
#define CapRound              2
#define CapProjecting         3

#define JoinMiter             0
#define JoinRound             1
#define JoinBevel             2

#define FillSolid             0
#define FillTiled             1
#define FillStippled          2
#define FillOpaqueStippled    3

#define EvenOddRule           0
#define WindingRule           1

#define ArcChord              0
#define ArcPieSlice           1

#define ClipByChildren        0
#define IncludeInferiors      1

/* Raster ops (subset) */
#define GXclear               0x0
#define GXcopy                0x3
#define GXxor                 0x6
#define GXor                  0x7
#define GXand                 0x1

/* XGCValues struct (subset sufficient for our IPC) */
typedef struct {
    int function;
    unsigned long plane_mask;
    unsigned long foreground;
    unsigned long background;
    int line_width;
    int line_style;
    int cap_style;
    int join_style;
    int fill_style;
    int fill_rule;
    int arc_mode;

    Pixmap tile;
    Pixmap stipple;
    int ts_x_origin;
    int ts_y_origin;

    Font font;
    int subwindow_mode;
    Bool graphics_exposures;
    int clip_x_origin;
    int clip_y_origin;
    Pixmap clip_mask;

    int dash_offset;
    char dashes;
} XGCValues;

/* NEW: GC creation and free */
GC XCreateGC(Display *display, Drawable d, unsigned long valuemask, XGCValues *values);
int XFreeGC(Display *display, GC gc);

#ifdef __cplusplus
}
#endif

#endif /* X11_XLIB_H */
