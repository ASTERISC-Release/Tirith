#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define BENCHMARK

#ifdef BENCHMARK
#include <sys/time.h>
#include <unistd.h>

static double current_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
}
#else
static double current_time(void) {
    return 1.0;
}
#endif

#ifndef M_PI
#define M_PI 3.14159265
#endif

#define NOP  0
#define EXIT 1
#define DRAW 2

static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;
static GLuint background_tex_id;
static GLuint gear_tex_id;

static GLboolean animate = GL_TRUE;
static GLfloat left, right, asp;

static GLuint load_texture(const char* filename) {
    int w, h, n;
    /* stbi_load works for JPG and PNG automatically */
    unsigned char* data = stbi_load(filename, &w, &h, &n, 0);
    if (!data) {
        fprintf(stderr, "Could not load texture file %s\n", filename);
        return 0;
    }

    fprintf(stderr, "umping texture %s (%dx%d, %d source channels)\n", filename, w, h, n);
    fprintf(stderr, "--- Code Level Header Dump (Simulated 4-byte/BGRX) ---\n");
    for (int i = 0; i < 64; i++) { // 64 pixels * 4 bytes = 256 bytes
        unsigned char r, g, b, x;
        if (n == 3) {
            r = data[i * 3 + 0];
            g = data[i * 3 + 1];
            b = data[i * 3 + 2];
            x = 0x00; // Driver padding
        } else {
            // Already 4 channels (RGBA/BGRA)
            r = data[i * 4 + 0];
            g = data[i * 4 + 1];
            b = data[i * 4 + 2];
            x = data[i * 4 + 3];
        }
        
        // Printing as R G B X to match your execbuf hex pattern
        fprintf(stderr, "%02x %02x %02x %02x ", r, g, b, x);
        if ((i + 1) % 4 == 0) fprintf(stderr, "\n");
    }

    // 2. Center Dump (256 bytes from the middle of the image)
    size_t center_pixel_idx = (size_t)(w * h / 2);
    fprintf(stderr, "--- Code Level Center Dump (Simulated 4-byte/BGRX) ---\n");
    for (int i = 0; i < 64; i++) {
        size_t curr_pixel = center_pixel_idx + i;
        unsigned char r, g, b, x;
        if (n == 3) {
            r = data[curr_pixel * 3 + 0];
            g = data[curr_pixel * 3 + 1];
            b = data[curr_pixel * 3 + 2];
            x = 0x00;
        } else {
            r = data[curr_pixel * 4 + 0];
            g = data[curr_pixel * 4 + 1];
            b = data[curr_pixel * 4 + 2];
            x = data[curr_pixel * 4 + 3];
        }
        fprintf(stderr, "%02x %02x %02x %02x ", r, g, b, x);
        if ((i + 1) % 4 == 0) fprintf(stderr, "\n");
    }

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    
    // Standard parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    /* Handle 3 channels (RGB) or 4 channels (RGBA) */
    GLenum format = (n == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return id;
}

static void gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth,
                 GLfloat tooth_depth) {
    GLint i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0;
    r2 = outer_radius + tooth_depth / 2.0;
    da = 2.0 * M_PI / teeth / 4.0;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gear_tex_id);
    glShadeModel(GL_FLAT);
    glColor3f(1.0, 1.0, 1.0); /* Ensure texture isn't tinted */

    /* Front face */
    glNormal3f(0.0, 0.0, 1.0);
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glTexCoord2f(0.5 + 0.5 * cos(angle) * r0 / r2, 0.5 + 0.5 * sin(angle) * r0 / r2);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
        glTexCoord2f(0.5 + 0.5 * cos(angle) * r1 / r2, 0.5 + 0.5 * sin(angle) * r1 / r2);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    }
    glEnd();

    /* Front sides of teeth */
    glBegin(GL_QUADS);
    for (i = 0; i < teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glTexCoord2f(0.5 + 0.5 * cos(angle) * r1 / r2, 0.5 + 0.5 * sin(angle) * r1 / r2);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
        glTexCoord2f(0.5 + 0.5 * cos(angle + da) * r2 / r2, 0.5 + 0.5 * sin(angle + da) * r2 / r2);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
        glTexCoord2f(0.5 + 0.5 * cos(angle + 2 * da) * r2 / r2, 0.5 + 0.5 * sin(angle + 2 * da) * r2 / r2);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
        glTexCoord2f(0.5 + 0.5 * cos(angle + 3 * da) * r1 / r2, 0.5 + 0.5 * sin(angle + 3 * da) * r1 / r2);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
    }
    glEnd();

    /* Back face */
    glNormal3f(0.0, 0.0, -1.0);
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glTexCoord2f(0.5 + 0.5 * cos(angle) * r1 / r2, 0.5 + 0.5 * sin(angle) * r1 / r2);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
        glTexCoord2f(0.5 + 0.5 * cos(angle) * r0 / r2, 0.5 + 0.5 * sin(angle) * r0 / r2);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    }
    glEnd();

    /* Outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        GLfloat s = (GLfloat)i / teeth;
        glTexCoord2f(s, 0.0); glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
        glTexCoord2f(s, 1.0); glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
        u = r2 * cos(angle + da) - r1 * cos(angle);
        v = r2 * sin(angle + da) - r1 * sin(angle);
        len = sqrt(u * u + v * v);
        u /= len; v /= len;
        glNormal3f(v, -u, 0.0);
        glTexCoord2f(s + 0.25 / teeth, 0.0); glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
        glTexCoord2f(s + 0.25 / teeth, 1.0); glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
        glNormal3f(cos(angle + 1.5 * da), sin(angle + 1.5 * da), 0.0);
        glTexCoord2f(s + 0.5 / teeth, 0.0); glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
        glTexCoord2f(s + 0.5 / teeth, 1.0); glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
        u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
        v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
        glNormal3f(v, -u, 0.0);
        glTexCoord2f(s + 0.75 / teeth, 0.0); glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
        glTexCoord2f(s + 0.75 / teeth, 1.0); glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
        glNormal3f(cos(angle), sin(angle), 0.0);
    }
    glTexCoord2f(1.0, 0.0); glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
    glTexCoord2f(1.0, 1.0); glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

static void draw_background(void) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, background_tex_id);
    glColor3f(1.0f, 1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-25.0f, -25.0f, -50.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 25.0f, -25.0f, -50.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 25.0f,  25.0f, -50.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-25.0f,  25.0f, -50.0f);
    glEnd();
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

static void draw(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_background();

    glPushMatrix();
    glTranslatef(0.0, 0.0, -40.0); 
    glRotatef(view_rotx, 1.0, 0.0, 0.0);
    glRotatef(view_roty, 0.0, 1.0, 0.0);
    glRotatef(view_rotz, 0.0, 0.0, 1.0);

    glCallList(gear1);
    glCallList(gear2);
    glCallList(gear3);

    /* Side Rectangle */
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, background_tex_id);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0); glVertex3f(7.0, -5.0, 2.0);
    glTexCoord2f(1.0, 0.0); glVertex3f(12.0, -5.0, 2.0);
    glTexCoord2f(1.0, 1.0); glVertex3f(12.0,  5.0, 2.0);
    glTexCoord2f(0.0, 1.0); glVertex3f(7.0,  5.0, 2.0);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);

    glPopMatrix();
}

static void draw_frame(Display* dpy, Window win) {
    static double tRot0 = -1.0;
    double dt, t = current_time();
    if (tRot0 < 0.0) tRot0 = t;
    dt = t - tRot0; tRot0 = t;

    if (animate) {
        angle += 70.0 * dt;
        if (angle > 3600.0) angle -= 3600.0;
    }

    /* Redraw display lists is not efficient, but we use the angle in the draw call */
    /* To keep gears moving, we wrap the draw calls in the rotation logic */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_background();
    glPushMatrix();
    glTranslatef(0.0, 0.0, -40.0);
    glRotatef(view_rotx, 1.0, 0.0, 0.0);
    glRotatef(view_roty, 0.0, 1.0, 0.0);
    glRotatef(view_rotz, 0.0, 0.0, 1.0);

    glPushMatrix();
    glTranslatef(-3.0, -2.0, 0.0);
    glRotatef(angle, 0.0, 0.0, 1.0);
    glCallList(gear1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3.1, -2.0, 0.0);
    glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
    glCallList(gear2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3.1, 4.2, 0.0);
    glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
    glCallList(gear3);
    glPopMatrix();

    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, background_tex_id);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0); glVertex3f(7.0, -5.0, 2.0);
    glTexCoord2f(1.0, 0.0); glVertex3f(12.0, -5.0, 2.0);
    glTexCoord2f(1.0, 1.0); glVertex3f(12.0,  5.0, 2.0);
    glTexCoord2f(0.0, 1.0); glVertex3f(7.0,  5.0, 2.0);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glPopMatrix();

    glXSwapBuffers(dpy, win);
}

static void reshape(int width, int height) {
    glViewport(0, 0, (GLint)width, (GLint)height);
    asp = (GLfloat)height / (GLfloat)width;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -asp, asp, 5.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

static void init(void) {
    static GLfloat pos[4] = {5.0, 5.0, 10.0, 0.0};
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);

    /* Alpha blending for PNG transparency */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    background_tex_id = load_texture("brickwall.jpg");
    gear_tex_id = load_texture("texture.png"); /* Changed to .png */

    gear1 = glGenLists(1);
    glNewList(gear1, GL_COMPILE);
    gear(1.0, 4.0, 1.0, 20, 0.7);
    glEndList();

    gear2 = glGenLists(1);
    glNewList(gear2, GL_COMPILE);
    gear(0.5, 2.0, 2.0, 10, 0.7);
    glEndList();

    gear3 = glGenLists(1);
    glNewList(gear3, GL_COMPILE);
    gear(1.3, 2.0, 0.5, 10, 0.7);
    glEndList();
}

static void make_window(Display* dpy, const char* name, int x, int y, int width, int height,
                        Window* winRet, GLXContext* ctxRet) {
    int attribs[] = {GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1,
                     GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 1, None};
    XVisualInfo* visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
    XSetWindowAttributes attr;
    attr.colormap = XCreateColormap(dpy, RootWindow(dpy, visinfo->screen), visinfo->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, visinfo->screen), x, y, width, height, 0,
                               visinfo->depth, InputOutput, visinfo->visual,
                               CWColormap | CWEventMask, &attr);
    XStoreName(dpy, win, name);
    GLXContext ctx = glXCreateContext(dpy, visinfo, NULL, True);
    XFree(visinfo);
    *winRet = win;
    *ctxRet = ctx;
}

int main(int argc, char* argv[]) {
    Display* dpy = XOpenDisplay(NULL);
    Window win;
    GLXContext ctx;
    make_window(dpy, "glxgears-textured", 0, 0, 600, 600, &win, &ctx);
    XMapWindow(dpy, win);
    glXMakeCurrent(dpy, win, ctx);
    init();
    reshape(600, 600);
    while (1) {
        while (XPending(dpy) > 0) {
            XEvent event;
            XNextEvent(dpy, &event);
            if (event.type == ConfigureNotify) reshape(event.xconfigure.width, event.xconfigure.height);
            if (event.type == KeyPress) {
                if (XLookupKeysym(&event.xkey, 0) == XK_Escape) return 0;
            }
        }
        draw_frame(dpy, win);
    }
    return 0;
}
