#ifndef __X11_CONSTANTS_H__
#define __X11_CONSTANTS_H__

/* Pixel color definitions - common X11 color values */
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

/* Gravity values */
#define ForgetGravity   0
#define NorthWestGravity 1
#define NorthGravity    2
#define NorthEastGravity 3
#define WestGravity     4
#define CenterGravity   5
#define EastGravity     6
#define SouthWestGravity 7
#define SouthGravity    8
#define SouthEastGravity 9
#define StaticGravity   10

/* Backing store values */
#define NotUseful       0
#define WhenMapped      1
#define Always          2

/* Event mask constants (you already have some, but here are more) */
#define KeyPressMask            (1L<<0)
#define KeyReleaseMask          (1L<<1)
#define ButtonPressMask         (1L<<2)
#define ButtonReleaseMask       (1L<<3)
#define EnterWindowMask         (1L<<4)
#define LeaveWindowMask         (1L<<5)
#define PointerMotionMask       (1L<<6)
#define PointerMotionHintMask   (1L<<7)
#define Button1MotionMask       (1L<<8)
#define Button2MotionMask       (1L<<9)
#define Button3MotionMask       (1L<<10)
#define Button4MotionMask       (1L<<11)
#define Button5MotionMask       (1L<<12)
#define ButtonMotionMask        (1L<<13)
#define KeymapStateMask         (1L<<14)
#define ExposureMask            (1L<<15)
#define VisibilityChangeMask    (1L<<16)
#define StructureNotifyMask     (1L<<17)
#define ResizeRedirectMask      (1L<<18)
#define SubstructureNotifyMask  (1L<<19)
#define SubstructureRedirectMask (1L<<20)
#define FocusChangeMask         (1L<<21)
#define PropertyChangeMask      (1L<<22)
#define ColormapChangeMask      (1L<<23)
#define OwnerGrabButtonMask     (1L<<24)

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

/* Useful helper function to create RGB pixel values */
static inline unsigned long MakePixel(unsigned char red, unsigned char green, unsigned char blue) {
    return ((unsigned long)red << 16) | ((unsigned long)green << 8) | (unsigned long)blue;
}

/* Common predefined colors */
// static inline unsigned long BlackPixel(void) { return BlackPixelValue; }
// static inline unsigned long WhitePixel(void) { return WhitePixelValue; }
// static inline unsigned long RedPixel(void) { return RedPixelValue; }
// static inline unsigned long GreenPixel(void) { return GreenPixelValue; }
// static inline unsigned long BluePixel(void) { return BluePixelValue; }

/* Add this with your other type definitions */
typedef int Status;
#define BadAlloc 1
#define BadColor 2
#define BadCursor 3
#define BadDrawable 4
#define BadFont 5
#define BadGC 6
#define BadIDChoice 7
#define BadImplementation 8
#define BadLength 9
#define BadMatch 10
#define BadName 11
#define BadPixmap 12
#define BadRequest 13
#define BadValue 14
#define BadWindow 15

#endif /* __X11_CONSTANTS_H__ */