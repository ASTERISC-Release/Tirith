/*
 * test_gl.c
 *
 * Simple GLX/OpenGL test that exercises XCreateWindow, XMapWindow,
 * XSelectInput, XDefaultRootWindow/XRootWindow, XConfigureWindow,
 * XUnmapWindow, XDestroyWindow, XFlush, XPending/XNextEvent.
 *
 * Controls:
 *   - Esc or 'q' : quit
 *   - 'u'        : unmap/remap the window (toggle)
 *
 * Build:
 *   gcc -o test_gl test_gl.c -lX11 -lGL -lm
 *
 * Run (if you use LD_PRELOAD wrapper):
 *   ./server_shm    # in another terminal
 *   LD_PRELOAD=./libx11ipc.so ./test_gl
 *
 * If wrappers are built into the test binary, run it directly while server_shm is running.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <GL/gl.h>
#include <GL/glx.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int running = 1;
static int mapped = 1;

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* Simple GL draw */
static void render(double t, int w, int h) {
    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (h>0) ? (float)w / (float)h : 1.0f;
    glOrtho(-aspect, aspect, -1.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef((float)(t * 60.0), 0.0f, 0.0f, 1.0f);

    glBegin(GL_TRIANGLES);
      glColor3f(1.0f, 0.2f, 0.2f);
      glVertex2f(0.0f, 0.6f);
      glColor3f(0.2f, 1.0f, 0.2f);
      glVertex2f(-0.6f, -0.6f);
      glColor3f(0.2f, 0.2f, 1.0f);
      glVertex2f(0.6f, -0.6f);
    glEnd();
}

/* Choose a GLX visual */
static XVisualInfo *choose_glx_visual(Display *dpy) {
    int attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        None
    };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
    return vi;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "XOpenDisplay failed\n");
        return 1;
    }
    fprintf(stderr, "Opened display: %p\n", (void*)dpy);

    /* Get root window via wrapper (exercises XDefaultRootWindow/XRootWindow) */
    Window root = XDefaultRootWindow(dpy);
    fprintf(stderr, "Default root window: 0x%lx\n", (unsigned long)root);
    Window root0 = XRootWindow(dpy, 0);
    fprintf(stderr, "RootWindow(screen 0): 0x%lx\n", (unsigned long)root0);

    /* Choose GLX visual */
    XVisualInfo *vi = choose_glx_visual(dpy);
    if (!vi) {
        fprintf(stderr, "glXChooseVisual failed; the server may not support GLX with the chosen attributes\n");
        /* Still continue: create a simple X window using default visual to test X calls */
    } else {
        fprintf(stderr, "Chosen visual: depth=%d, id=0x%lx\n", vi->depth, (unsigned long)vi->visualid);
    }

    /* Create X window using XCreateWindow wrapper (full signature) */
    int win_x = 100, win_y = 100;
    unsigned int win_w = 640, win_h = 480, border_width = 0;
    //int depth = (vi ? vi->depth : CopyFromParent);
    int depth = vi ? vi->depth : DefaultDepth(dpy, DefaultScreen(dpy));
    unsigned int wclass = InputOutput;
    Visual *visual = (vi ? vi->visual : NULL);

    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof(swa));
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask;
    swa.background_pixel = 0;
    swa.border_pixel = 0;
    unsigned long valuemask = CWBackPixel | CWBorderPixel | CWEventMask;

    fprintf(stderr, "Creating window at %dx%d size %dx%d depth %d\n",
            win_x, win_y, win_w, win_h, depth);

    Window win = XCreateWindow(dpy, root,
                               win_x, win_y, win_w, win_h,
                               border_width,
                               depth,
                               wclass,
                               visual,
                               valuemask,
                               &swa);
    if (!win) {
        fprintf(stderr, "XCreateWindow failed\n");
        XCloseDisplay(dpy);
        return 1;
    }
    fprintf(stderr, "Created window: 0x%lx\n", (unsigned long)win);

    /* Set WM name (best-effort) using XStoreName if available in your wrappers.
       If not available, it's okay; this call is optional. */
#ifdef HAVE_XSTORENAME
    XStoreName(dpy, win, "IPC GL Test Window");
#endif

    /* Select input events (exercise XSelectInput) */
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask);

    /* Map the window (exercise XMapWindow) */
    XMapWindow(dpy, win);
    XFlush(dpy);

    /* Setup GLX context if we have a visual */
    GLXContext ctx = NULL;
    if (vi) {
        ctx = glXCreateContext(dpy, vi, NULL, GL_TRUE);
        if (!ctx) {
            fprintf(stderr, "glXCreateContext failed\n");
        } else {
            if (!glXMakeCurrent(dpy, win, ctx)) {
                fprintf(stderr, "glXMakeCurrent failed\n");
                glXDestroyContext(dpy, ctx);
                ctx = NULL;
            } else {
                fprintf(stderr, "GLX context created and made current\n");
            }
        }
    } else {
        fprintf(stderr, "No GLX visual: skipping GL context creation\n");
    }

    /* Main loop: simple rotate animation and event handling */
    double t0 = now_seconds();
    int width = (int)win_w, height = (int)win_h;

    while (running) {
        /* handle pending events */
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            if (ev.type == Expose) {
                /* repaint on expose */
                /* nothing special; will paint below */
            } else if (ev.type == ConfigureNotify) {
                XConfigureEvent *ce = (XConfigureEvent*)&ev;
                width = ce->width;
                height = ce->height;
                fprintf(stderr, "ConfigureNotify: w=%d h=%d\n", width, height);
            } else if (ev.type == ButtonPress) {
                fprintf(stderr, "ButtonPress: button=%d\n", ev.xbutton.button);
            } else if (ev.type == KeyPress) {
                KeySym ks;
                char buf[32];
                int len = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);
                (void)len;
                if (ks == XK_Escape || ks == XK_q) {
                    running = 0;
                } else if (buf[0] == 'u') {
                    /* toggle unmap / map */
                    if (mapped) {
                        fprintf(stderr, "Unmapping window\n");
                        XUnmapWindow(dpy, win);
                        XFlush(dpy);
                        mapped = 0;
                    } else {
                        fprintf(stderr, "Remapping window\n");
                        XMapWindow(dpy, win);
                        XFlush(dpy);
                        mapped = 1;
                    }
                } else if (buf[0] == 'r') {
                    /* Resize via XConfigureWindow to exercise configure */
                    XWindowChanges wc;
                    wc.x = 0; wc.y = 0;
                    wc.width = width + 50;
                    wc.height = height + 30;
                    wc.border_width = 0;
                    XConfigureWindow(dpy, win, CWWidth | CWHeight, &wc);
                    XFlush(dpy);
                }
            }
        }

        /* render frame */
        double t = now_seconds() - t0;
        if (ctx) {
            render(t, width, height);
            glXSwapBuffers(dpy, win);
        } else {
            /* no GL: we can still use X drawing to show simple expose,
               but we purposely keep it simple here. */
        }

        /* small sleep to limit CPU usage */
        struct timespec req = {0, 16000000}; /* ~16ms */
        nanosleep(&req, NULL);
    }

    /* cleanup */
    if (ctx) {
        glXMakeCurrent(dpy, None, NULL);
        glXDestroyContext(dpy, ctx);
    }

    XUnmapWindow(dpy, win);
    XDestroyWindow(dpy, win);
    XFlush(dpy);
    XCloseDisplay(dpy);

    fprintf(stderr, "Exiting test program\n");
    return 0;
}

