// glx_cube.cpp
// Rotating cube using X11 + GLX (no GLFW, no GLAD).
// Compile with: g++ -std=c++17 glx_cube.cpp -o glx_cube -lX11 -lGL -lm

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <chrono>

// Simple helper: print and exit
static void fatal(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    std::exit(1);
}

// Draw a colored cube using immediate mode (fixed-function)
void drawCube() {
    glBegin(GL_QUADS);
    // Front (red)
    glColor3f(1,0,0);
    glVertex3f(-1,-1, 1);
    glVertex3f( 1,-1, 1);
    glVertex3f( 1, 1, 1);
    glVertex3f(-1, 1, 1);
    // Back (green)
    glColor3f(0,1,0);
    glVertex3f(-1,-1,-1);
    glVertex3f(-1, 1,-1);
    glVertex3f( 1, 1,-1);
    glVertex3f( 1,-1,-1);
    // Left (blue)
    glColor3f(0,0,1);
    glVertex3f(-1,-1,-1);
    glVertex3f(-1,-1, 1);
    glVertex3f(-1, 1, 1);
    glVertex3f(-1, 1,-1);
    // Right (yellow)
    glColor3f(1,1,0);
    glVertex3f(1,-1,-1);
    glVertex3f(1, 1,-1);
    glVertex3f(1, 1, 1);
    glVertex3f(1,-1, 1);
    // Top (cyan)
    glColor3f(0,1,1);
    glVertex3f(-1,1,-1);
    glVertex3f(-1,1, 1);
    glVertex3f( 1,1, 1);
    glVertex3f( 1,1,-1);
    // Bottom (magenta)
    glColor3f(1,0,1);
    glVertex3f(-1,-1,-1);
    glVertex3f( 1,-1,-1);
    glVertex3f( 1,-1, 1);
    glVertex3f(-1,-1, 1);
    glEnd();
}

int main() {
    // ---------- Open X display ----------
    Display* xdisp = XOpenDisplay(nullptr);
    if (!xdisp) fatal("Failed to open X display.");

    int screen = DefaultScreen(xdisp);

    // Choose visual info that supports GLX
    static int visual_attribs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };
    XVisualInfo* vi = glXChooseVisual(xdisp, screen, visual_attribs);
    if (!vi) fatal("No appropriate visual found (glXChooseVisual returned null).");

    // Create X colormap and window
    Colormap cmap = XCreateColormap(xdisp, RootWindow(xdisp, vi->screen), vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    unsigned long mask = CWColormap | CWEventMask;
    Window win = XCreateWindow(
        xdisp,
        RootWindow(xdisp, vi->screen),
        0, 0, 800, 600,
        0, vi->depth, InputOutput, vi->visual,
        mask, &swa
    );

    if (!win) fatal("Failed to create X window.");

    // Set window title
    XStoreName(xdisp, win, "GLX Rotating Cube");

    // Prepare to handle WM_DELETE_WINDOW
    Atom wmDelete = XInternAtom(xdisp, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(xdisp, win, &wmDelete, 1);

    // Map (show) the window
    XMapWindow(xdisp, win);

    // ---------- Create GLX context ----------
    GLXContext ctx = glXCreateContext(xdisp, vi, nullptr, GL_TRUE);
    if (!ctx) fatal("Failed to create GLX context.");

    // Make context current
    if (!glXMakeCurrent(xdisp, win, ctx)) fatal("glXMakeCurrent failed.");

    // ---------- GL state ----------
    glEnable(GL_DEPTH_TEST);

    // Timing
    auto lastT = std::chrono::steady_clock::now();
    float angle = 0.0f;

    // Query initial window size
    XWindowAttributes gwa;
    XGetWindowAttributes(xdisp, win, &gwa);
    int width = gwa.width, height = gwa.height;
    if (height == 0) height = 1;
    glViewport(0, 0, width, height);

    // Simple projection setup helper
    auto setProjection = [&](int w, int h) {
        if (h == 0) h = 1;
        float aspect = float(w) / float(h);
        glViewport(0, 0, w, h);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        // Build a simple perspective matrix with glFrustum
        float fovy = 45.0f; // degrees
        float znear = 0.1f, zfar = 100.0f;
        float top = tanf((fovy * 3.14159265f / 180.0f) * 0.5f) * znear;
        float right = top * aspect;
        glFrustum(-right, right, -top, top, znear, zfar);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    };

    setProjection(width, height);

    // ---------- Event & render loop ----------
    bool running = true;
    while (running) {
        // Process all X events
        while (XPending(xdisp)) {
            XEvent xev;
            XNextEvent(xdisp, &xev);
            switch (xev.type) {
                case Expose:
                    // Redraw later
                    break;
                case ConfigureNotify:
                    // Window resized
                    width = xev.xconfigure.width;
                    height = xev.xconfigure.height;
                    setProjection(width, height);
                    break;
                case KeyPress: {
                    KeySym ks = XLookupKeysym(&xev.xkey, 0);
                    if (ks == XK_Escape) running = false;
                    break;
                }
                case ClientMessage: {
                    if ((Atom)xev.xclient.data.l[0] == wmDelete) running = false;
                    break;
                }
                default:
                    break;
            }
        }

        // Timing
        auto nowT = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = nowT - lastT;
        lastT = nowT;
        float dt = elapsed.count();

        // update
        angle += 30.0f * dt; // degrees per second

        // render
        glClearColor(0.08f, 0.12f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -6.0f);
        glRotatef(angle, 1.0f, 1.0f, 0.3f);

        drawCube();

        // swap buffers
        glXSwapBuffers(xdisp, win);
    }

    // ---------- Cleanup ----------
    glXMakeCurrent(xdisp, None, nullptr);
    glXDestroyContext(xdisp, ctx);
    XDestroyWindow(xdisp, win);
    XCloseDisplay(xdisp);
    return 0;
}
