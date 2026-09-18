#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct {
    float x, y, w, h;
    GLuint tex_id;
} Rectangle;

static Rectangle rects[100];
static int num_rects = 0;
static GLuint tex1_id, tex2_id, bg_tex_id;

// Adjust this to scale the "HI" text
static const float BLOCK_SIZE = 30.0f;

static GLuint load_texture(const char* filename) {
    int w, h, n;
    unsigned char* data = stbi_load(filename, &w, &h, &n, 0);
    if (!data) {
        fprintf(stderr, "Could not load texture %s\n", filename);
        return 0;
    }

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = (n == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return id;
}

static void add_rect(float x, float y, float w, float h, GLuint tex) {
    rects[num_rects++] = (Rectangle){x, y, w, h, tex};
}

static void init(void) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    tex1_id   = load_texture("brickwall.jpg");
    tex2_id   = load_texture("texture.png");
    bg_tex_id = load_texture("bg.png");

    // Calculate centering based on total size
    const float total_width  = 8 * BLOCK_SIZE;  // H is 5 wide, gap 1, I is 1
    const float total_height = 7 * BLOCK_SIZE;
    const float start_x      = (300 - total_width) / 2;
    const float start_y      = (300 - total_height) / 2;

    // Letter "H" - left vertical
    for (int i = 0; i < 7; i++) {
        add_rect(start_x, start_y + i * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE,
                 (num_rects % 2) ? tex1_id : tex2_id);
    }

    // Letter "H" - middle horizontal
    for (int i = 1; i < 4; i++) {
        add_rect(start_x + i * BLOCK_SIZE, start_y + 3 * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE,
                 (num_rects % 2) ? tex1_id : tex2_id);
    }

    // Letter "H" - right vertical
    for (int i = 0; i < 7; i++) {
        add_rect(start_x + 4 * BLOCK_SIZE, start_y + i * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE,
                 (num_rects % 2) ? tex1_id : tex2_id);
    }

    // Letter "I" - vertical
    for (int i = 0; i < 7; i++) {
        add_rect(start_x + 7 * BLOCK_SIZE, start_y + i * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE,
                 (num_rects % 2) ? tex1_id : tex2_id);
    }
}

static void draw_textured_rect(float x, float y, float w, float h, GLuint tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(x, y);
    glTexCoord2f(1, 0);
    glVertex2f(x + w, y);
    glTexCoord2f(1, 1);
    glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1);
    glVertex2f(x, y + h);
    glEnd();
}

static void draw_frame(Display* dpy, Window win) {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    if (bg_tex_id) {
        draw_textured_rect(0, 0, 300, 300, bg_tex_id);
    }

    for (int i = 0; i < num_rects; i++) {
        draw_textured_rect(rects[i].x, rects[i].y, rects[i].w, rects[i].h, rects[i].tex_id);
    }

    glXSwapBuffers(dpy, win);
}

int main(void) {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 1;

    int attribs[]    = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, None};
    XVisualInfo* vis = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);

    XSetWindowAttributes swa = {
        .colormap   = XCreateColormap(dpy, RootWindow(dpy, vis->screen), vis->visual, AllocNone),
        .event_mask = StructureNotifyMask | KeyPressMask};

    Window win = XCreateWindow(dpy, RootWindow(dpy, vis->screen), 0, 0, 300, 300, 0, vis->depth,
                               InputOutput, vis->visual, CWColormap | CWEventMask, &swa);

    XMapWindow(dpy, win);
    XStoreName(dpy, win, "HI");

    GLXContext ctx = glXCreateContext(dpy, vis, NULL, True);
    glXMakeCurrent(dpy, win, ctx);

    glViewport(0, 0, 300, 300);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 300, 0, 300, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    init();

    while (1) {
        while (XPending(dpy) > 0) {
            XEvent event;
            XNextEvent(dpy, &event);
            if (event.type == KeyPress && XLookupKeysym(&event.xkey, 0) == XK_Escape) {
                return 0;
            }
        }
        draw_frame(dpy, win);
    }
}
