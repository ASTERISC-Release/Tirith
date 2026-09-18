#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

/* Dump raw framebuffer (RGB) into PPM */
static void dump_ppm(const char *filename, int width, int height)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "PPM: Failed to open file %s\n", filename);
        return;
    }

    // Allocate buffer for RGB pixels
    unsigned char *pixels = malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "PPM: Failed to allocate pixel buffer\n");
        fclose(fp);
        return;
    }

    // Read from bottom-left origin buffer
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // Check GL errors
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        fprintf(stderr, "PPM: glReadPixels error: 0x%x\n", e);
    }

    // Write basic PPM header
    // P6 = Binary RGB (fast and compact)
    fprintf(fp, "P6\n%d %d\n255\n", width, height);

    // Flip vertically while writing
    for (int y = height - 1; y >= 0; y--) {
        fwrite(pixels + y * width * 3, 3, width, fp);
    }

    fclose(fp);
    free(pixels);

    fprintf(stderr, "PPM: Wrote %s\n", filename);
}


static void print_gl_errors(const char *stage)
{
    GLenum e;
    while ((e = glGetError()) != GL_NO_ERROR) {
        fprintf(stderr, "[GLERR] %s: 0x%x\n", stage, e);
    }
}

static void check_fbo_status(const char *tag)
{
    GLint fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    fprintf(stderr, "[FBO] %s: GL_FRAMEBUFFER_BINDING = %d\n", tag, fbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    fprintf(stderr, "[FBO] %s: status = 0x%x\n", tag, status);

    print_gl_errors("check_fbo_status-end");
}
static void dump_context_info(const char *tag)
{
    const GLubyte *ver   = glGetString(GL_VERSION);
    const GLubyte *rend  = glGetString(GL_RENDERER);
    const GLubyte *vend  = glGetString(GL_VENDOR);
    const GLubyte *shver = glGetString(GL_SHADING_LANGUAGE_VERSION);

    fprintf(stderr, "\n===== CONTEXT INFO (%s) =====\n", tag);
    fprintf(stderr, "GL_VERSION   : %s\n", ver   ? (const char*)ver   : "(null)");
    fprintf(stderr, "GL_RENDERER  : %s\n", rend  ? (const char*)rend  : "(null)");
    fprintf(stderr, "GL_VENDOR    : %s\n", vend  ? (const char*)vend  : "(null)");
    fprintf(stderr, "GLSL_VERSION : %s\n", shver ? (const char*)shver : "(null)");

    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    fprintf(stderr, "GL version   : %d.%d\n", major, minor);

#ifdef GL_CONTEXT_PROFILE_MASK
    GLint profile_mask = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_mask);
    fprintf(stderr, "Profile mask : 0x%x", profile_mask);
#   ifdef GL_CONTEXT_CORE_PROFILE_BIT
    if (profile_mask & GL_CONTEXT_CORE_PROFILE_BIT)
        fprintf(stderr, " [CORE]");
#   endif
#   ifdef GL_CONTEXT_COMPATIBILITY_PROFILE_BIT
    if (profile_mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
        fprintf(stderr, " [COMPAT]");
#   endif
    fprintf(stderr, "\n");
#endif

    print_gl_errors("dump_context_info-end");
}

static void dump_full_state(const char *tag)
{
    fprintf(stderr, "\n================= STATE DUMP (%s) =================\n", tag);

    // Context info
    const GLubyte *ver   = glGetString(GL_VERSION);
    const GLubyte *rend  = glGetString(GL_RENDERER);
    const GLubyte *vend  = glGetString(GL_VENDOR);
    const GLubyte *shver = glGetString(GL_SHADING_LANGUAGE_VERSION);

    fprintf(stderr, "GL_VERSION   : %s\n", ver   ? (const char*)ver   : "(null)");
    fprintf(stderr, "GL_RENDERER  : %s\n", rend  ? (const char*)rend  : "(null)");
    fprintf(stderr, "GL_VENDOR    : %s\n", vend  ? (const char*)vend  : "(null)");
    fprintf(stderr, "GLSL_VERSION : %s\n", shver ? (const char*)shver : "(null)");

    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    fprintf(stderr, "GL version   : %d.%d\n", major, minor);

#ifdef GL_CONTEXT_PROFILE_MASK
    GLint profile_mask = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_mask);
    fprintf(stderr, "Profile mask : 0x%x\n", profile_mask);
#endif

    // Program / FBO
    GLint prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    fprintf(stderr, "CURRENT_PROGRAM  : %d\n", prog);

    GLint fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    fprintf(stderr, "FRAMEBUFFER_BINDING : %d\n", fbo);

    GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    fprintf(stderr, "FBO status      : 0x%x\n", fb_status);

    // Viewport / scissor / polygon mode
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    fprintf(stderr, "Viewport        : %d %d %d %d\n",
            viewport[0], viewport[1], viewport[2], viewport[3]);

    GLboolean scissor = GL_FALSE;
    GLint sc_box[4];
    glGetBooleanv(GL_SCISSOR_TEST, &scissor);
    glGetIntegerv(GL_SCISSOR_BOX, sc_box);
    fprintf(stderr, "Scissor         : enabled=%d box=%d %d %d %d\n",
            scissor, sc_box[0], sc_box[1], sc_box[2], sc_box[3]);

    GLint poly_mode[2];
    glGetIntegerv(GL_POLYGON_MODE, poly_mode);
    fprintf(stderr, "Polygon mode    : front=%d back=%d\n",
            poly_mode[0], poly_mode[1]);

    // Fixed-function bits that can wreck colors
    GLboolean lighting = GL_FALSE, texturing = GL_FALSE;
    glGetBooleanv(GL_LIGHTING, &lighting);
    glGetBooleanv(GL_TEXTURE_2D, &texturing);
    fprintf(stderr, "LIGHTING        : %d\n", lighting);
    fprintf(stderr, "TEXTURE_2D      : %d\n", texturing);

    GLint shade_model = 0;
    glGetIntegerv(GL_SHADE_MODEL, &shade_model);
    fprintf(stderr, "SHADE_MODEL     : %d\n", shade_model);

    print_gl_errors("dump_full_state-end");
}
static void
usage(void)
{
   printf("Usage:\n");
   printf("  -display <displayname>  set the display to run on\n");
   printf("  -stereo                 run in stereo mode\n");
   printf("  -samples N              run in multisample mode with at least N samples\n");
   printf("  -fullscreen             run in fullscreen mode\n");
   printf("  -info                   display OpenGL renderer info\n");
   printf("  -geometry WxH+X+Y       window geometry\n");
}
 
void print_block(){
             GLubyte block[8*8*4]; // Buffer for 8x8 pixels (4 bytes/pixel)
int cx = 1920/2;     // Center X coordinate
int cy = 1080/2;    // Center Y coordinate
glReadPixels(cx - 4, cy - 4, 8, 8, GL_RGB, GL_UNSIGNED_BYTE, block);
fprintf(stderr, "---- FBO TEST BLOCK 8x8 @ (%d,%d) ----\n", cx, cy);

// The GL coordinate system typically places (0,0) at the bottom-left,
// so we print from top to bottom (j=7 down to 0) to visualize it correctly.
for (int j = 7; j >= 0; --j) {
    for (int i = 0; i < 8; ++i) {
        int idx = 4 * (j*8 + i);
        // Print R G B A values in hexadecimal
        fprintf(stderr, "(%02x %02x %02x %02x) ",
        block[idx+0], block[idx+1], block[idx+2], block[idx+3]);
    }
    fprintf(stderr, "\n");
}
}
static void dump_write_masks(const char *tag)
{
    GLboolean colorMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);

    GLboolean depthMask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);

    GLint drawBuf;
    glGetIntegerv(GL_DRAW_BUFFER, &drawBuf);

    fprintf(stderr, "\n===== WRITE MASKS (%s) =====\n", tag);
    fprintf(stderr, "ColorMask   : R=%d G=%d B=%d A=%d\n",
            colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    fprintf(stderr, "DepthMask   : %d\n", depthMask);
    fprintf(stderr, "DrawBuffer  : 0x%x\n", drawBuf);
}

int
main(int argc, char *argv[])
{
   glXMakeCurrent(NULL, NULL, NULL);
   dump_full_state("AFTER MAKE CURRENT");
    GLuint quad_list = glGenLists(1);
    glViewport(0, 0, 1920, 1080);


        glNewList(quad_list, GL_COMPILE);
        glBegin(GL_QUADS);
            glVertex2f(-0.5f, -0.5f);
            glVertex2f( 0.5f, -0.5f);
            glVertex2f( 0.5f,  0.5f);
            glVertex2f(-0.5f,  0.5f);
        glEnd();
    glEndList();


for (int frame = 0; frame < 100000; ++frame) {
        int cur = frame % 2;
        glDisable(GL_DEPTH_TEST);
glDisable(GL_STENCIL_TEST);
glDisable(GL_SCISSOR_TEST);
glDisable(GL_CULL_FACE);
glDisable(GL_BLEND);
glViewport(0, 0, 1920, 1080);


glMatrixMode(GL_MODELVIEW);
glLoadIdentity();
glMatrixMode(GL_PROJECTION);
glLoadIdentity();

glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT);

// *** simplest possible immediate-mode box ***
glColor3f(1, 0, 0);  // solid red

    // glBegin(GL_QUADS);
    //     glVertex2f(-0.5f, -0.5f);
    //     glVertex2f( 0.5f, -0.5f);
    //     glVertex2f( 0.5f,  0.5f);
    //     glVertex2f(-0.5f,  0.5f);
    // glEnd();        

        glCallList(quad_list);
        /*
            [pid 124684] ioctl(5, DRM_IOCTL_SYNCOBJ_WAIT, 0x7ffc48fc78d0) = -1 ETIME (Timer expired)
            [pid 124684] ioctl(5, DRM_IOCTL_SYNCOBJ_WAIT, 0x7ffc48fc78d0) = 0  
        */
        // /* GPU sync - ensure GPU finished writing this buffer */
        glFlush();
        check_fbo_status("AFTER DRAW");
        print_block();
        dump_ppm("/host_tmp/schoooo.ppm", 1920, 1080);
        glXSwapBuffers(NULL, NULL);
        // return 0;


}
   return 0;
}

