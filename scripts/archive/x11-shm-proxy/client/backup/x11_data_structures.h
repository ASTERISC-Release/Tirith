#ifndef X11_DATA_STRUCTURES_H
#define X11_DATA_STRUCTURES_H

/* X11 structures and constants - defined locally */
#define True 1
#define False 0
#define None 0L
#define CurrentTime 0L

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

/* Stack modes */
#define Above 0
#define Below 1
#define TopIf 2
#define BottomIf 3
#define Opposite 4

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

/* Event structures */
/* Event structures */
typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
} XAnyEvent;

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

typedef union _XEvent {
    int type;
    XAnyEvent xany;
    XKeyEvent xkey;
    XButtonEvent xbutton;
    XExposeEvent xexpose;
    XClientMessageEvent xclient;
    /* Add other event types as needed */
    long pad[24];
} XEvent;

/* XWMHints structure */
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

/* XSizeHints structure */
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

/* XSetWindowAttributes structure */
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

/* XWindowChanges structure */
typedef struct {
    int x, y;
    int width, height;
    int border_width;
    Window sibling;
    int stack_mode;
} XWindowChanges;

typedef void* XPointer;

/* XComposeStatus structure */
typedef struct _XComposeStatus {
    XPointer compose_ptr;
    int chars_matched;
} XComposeStatus;

/* Add DefaultScreen macro */
#define DefaultScreen(d) XDefaultScreen(d)

/* Add other helper macros */
#define DefaultRootWindow(d) ((d)->screens[XDefaultScreen(d)].root)
#define RootWindow(d, scr) ((d)->screens[scr].root)
#define BlackPixel(d, scr) 0x000000
#define WhitePixel(d, scr) 0xFFFFFF
#define DisplayWidth(d, scr) ((d)->screens[scr].width)
#define DisplayHeight(d, scr) ((d)->screens[scr].height)
#define ScreenOfDisplay(d, scr) (&(d)->screens[scr])

#endif /* X11_DATA_STRUCTURES_H */