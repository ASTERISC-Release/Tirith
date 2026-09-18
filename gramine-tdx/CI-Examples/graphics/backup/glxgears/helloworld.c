/*
 * triangle_glx.c
 *
 * A tiny OpenGL/GLX program that opens an X‑window and draws a coloured
 * triangle.  It demonstrates the bare‑bones steps required to use GLX:
 *
 *   1. Open a connection to the X server (XOpenDisplay)
 *   2. Choose an appropriate visual with glXChooseVisual()
 *   3. Create an X window that uses that visual
 *   4. Create an OpenGL rendering context (glXCreateContext)
 *   5. Make the context current (glXMakeCurrent)
 *   6. Enter the event loop, draw each frame, swap buffers.
 *
 * Compile with:
 *
 *     gcc -Wall -O2 triangle_glx.c -lX11 -lGL -o triangle_glx
 *
 * Run:
 *
 *     ./triangle_glx
 *
 * Press the **Esc** key or close the window to exit.
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helper: abort with an error message                                 */
static void die(const char *msg)
{
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------ */
/* Simple OpenGL rendering function – draws a static triangle          */
static void render(void)
{
    /* Clear the colour buffer */
    glClear(GL_COLOR_BUFFER_BIT);

    /* Draw a coloured triangle */
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);   /* Red   */
        glVertex2f(-0.6f, -0.4f);
        glColor3f(0.0f, 1.0f, 0.0f);   /* Green */
        glVertex2f(0.6f, -0.4f);
        glColor3f(0.0f, 0.0f, 1.0f);   /* Blue  */
        glVertex2f(0.0f, 0.6f);
    glEnd();
}

/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    fprintf(stderr, "Executing main\n");

    /* -------------------------------------------------------------- *
     * 1. Open connection to the X server                               *
     * -------------------------------------------------------------- */
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) die("Cannot open display");

    /* -------------------------------------------------------------- *
     * 2. Choose an appropriate visual (RGBA, double‑buffered)        *
     * -------------------------------------------------------------- */
    int screen = DefaultScreen(dpy);

    fprintf(stderr, "Using display %s, screen %d\n",
            XDisplayName(NULL), screen);

    int glxAttribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE,   8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE,  8,
        GLX_DEPTH_SIZE, 24,
        None
    };

    XVisualInfo *vi = glXChooseVisual(dpy, screen, glxAttribs);
    if (!vi) die("No appropriate visual found");

    fprintf(stderr, "Using display %s, screen %d\n",
            XDisplayName(NULL), screen);


    /* -------------------------------------------------------------- *
     * 3. Create a colormap and the X window                           *
     * -------------------------------------------------------------- */
    Colormap cmap = XCreateColormap(dpy,
                                    RootWindow(dpy, vi->screen),
                                    vi->visual,
                                    AllocNone);

    fprintf(stderr, "Using display %s, screen %d\n",
            XDisplayName(NULL), screen);


    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    Window win = XCreateWindow(dpy,
                               RootWindow(dpy, vi->screen),
                               0, 0,                /* x, y          */
                               800, 600,            /* width, height */
                               0,                   /* border width  */
                               vi->depth,
                               InputOutput,
                               vi->visual,
                               CWColormap | CWEventMask,
                               &swa);
    if (!win) die("Failed to create window");

    fprintf(stderr, "Using display %s, screen %d\n",
            XDisplayName(NULL), screen);


    XStoreName(dpy, win, "GLX Triangle – press Esc to quit");
    XMapWindow(dpy, win);               /* Show the window */

    /* -------------------------------------------------------------- *
     * 4. Create an OpenGL rendering context                           *
     * -------------------------------------------------------------- */
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    if (!glc) die("Failed to create GLX context");


    fprintf(stderr, "Create Context %s, screen %d\n",
            XDisplayName(NULL), screen);


    /* -------------------------------------------------------------- *
     * 5. Make the context current for the created window              *
     * -------------------------------------------------------------- */
    if (!glXMakeCurrent(dpy, win, glc))
        die("Could not make GLX context current");

    /* -------------------------------------------------------------- *
     * 6. Set up basic OpenGL state                                    *
     * -------------------------------------------------------------- */
    glViewport(0, 0, 800, 600);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);   /* Simple orthographic projection */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    /* -------------------------------------------------------------- *
     * 7. Event / render loop                                           *
     * -------------------------------------------------------------- */
    bool running = true;
    while (running) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            switch (ev.type) {
                case Expose:
                    /* Window exposed – we can redraw immediately */
                    break;
                case ConfigureNotify:
                    /* Window resized – update the viewport */
                    glViewport(0, 0,
                               ((XConfigureEvent *)&ev)->width,
                               ((XConfigureEvent *)&ev)->height);
                    break;
                case KeyPress: {
                    /* Exit when Esc (keycode 9 on most keyboards) is pressed */
                    XKeyEvent *kev = (XKeyEvent *)&ev;
                    KeySym ks = XLookupKeysym(kev, 0);
                    if (ks == XK_Escape) {
                        running = false;
                    }
                    break;
                }
                case ClientMessage:
                    /* Handle WM_DELETE_WINDOW (window manager close) */
                    running = false;
                    break;
            }
        }

        /* Render a frame */
        render();

        /* Swap buffers (double‑buffered visual) */
        glXSwapBuffers(dpy, win);
    }

    /* -------------------------------------------------------------- *
     * 8. Clean‑up                                                     *
     * -------------------------------------------------------------- */
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    return 0;
}
