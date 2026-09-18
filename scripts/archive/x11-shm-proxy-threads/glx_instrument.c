#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

int main(int argc, char **argv) {
    const char *dname = getenv("DISPLAY");
    if (!dname) dname = ":0";
    Display *dpy = XOpenDisplay(dname);
    if (!dpy) {
        fprintf(stderr, "failed to open display %s\n", dname);
        return 1;
    }
    printf("opened display %p\n", (void*)dpy);
    int screen = DefaultScreen(dpy);
    int attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vis = glXChooseVisual(dpy, screen, attribs);
    printf("glXChooseVisual returned %p\n", (void*)vis);
    GLXContext ctx = glXCreateContext(dpy, vis, NULL, True);
    printf("glXCreateContext returned %p\n", (void*)ctx);
    if (ctx) glXDestroyContext(dpy, ctx);
    XCloseDisplay(dpy);
    return 0;
}
