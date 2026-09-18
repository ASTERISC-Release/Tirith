#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

// #ifndef GLX_MESA_swap_control
// #define GLX_MESA_swap_control 1
// typedef int (*PFNGLXGETSWAPINTERVALMESAPROC)(void);
// #endif


// #define BENCHMARK

// #ifdef BENCHMARK

// /* XXX this probably isn't very portable */

// #include <sys/time.h>
// #include <unistd.h>

// /* return current time (in seconds) */
// static double
// current_time(void)
// {
//    struct timeval tv;
// #ifdef __VMS
//    (void) gettimeofday(&tv, NULL );
// #else
//    struct timezone tz;
//    (void) gettimeofday(&tv, &tz);
// #endif
//    return (double) tv.tv_sec + tv.tv_usec / 1000000.0;
// }

// #else /*BENCHMARK*/

// /* dummy */
// static double
// current_time(void)
// {
//    /* update this function for other platforms! */
//    static double t = 0.0;
//    static int warn = 1;
//    if (warn) {
//       fprintf(stderr, "Warning: current_time() not implemented!!\n");
//       warn = 0;
//    }
//    return t += 1.0;
// }

// #endif /*BENCHMARK*/



// #ifndef M_PI
// #define M_PI 3.14159265
// #endif


// /** Event handler results: */
// #define NOP 0
// #define EXIT 1
// #define DRAW 2

// static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
// static GLint gear1, gear2, gear3;
// static GLfloat angle = 0.0;

// static GLboolean fullscreen = GL_FALSE;	/* Create a single fullscreen window */
// static GLboolean stereo = GL_FALSE;	/* Enable stereo.  */
// static GLint samples = 0;               /* Choose visual with at least N samples. */
// static GLboolean animate = GL_TRUE;	/* Animation */
// static GLfloat eyesep = 5.0;		/* Eye separation. */
// static GLfloat fix_point = 40.0;	/* Fixation point distance.  */
// static GLfloat left, right, asp;	/* Stereo frustum params.  */
// /* Dump raw framebuffer (RGB) into PPM */
// static void dump_ppm(const char *filename, int width, int height)
// {
//    glFlush();
//     FILE *fp = fopen(filename, "wb");
//     if (!fp) {
//         fprintf(stderr, "PPM: Failed to open file %s\n", filename);
//         return;
//     }

//     // Allocate buffer for RGB pixels
//     unsigned char *pixels = malloc(width * height * 3);
//     if (!pixels) {
//         fprintf(stderr, "PPM: Failed to allocate pixel buffer\n");
//         fclose(fp);
//         return;
//     }

//     // Read from bottom-left origin buffer
//     glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

//     // Check GL errors
//     GLenum e = glGetError();
//     if (e != GL_NO_ERROR) {
//         fprintf(stderr, "PPM: glReadPixels error: 0x%x\n", e);
//     }

//     // Write basic PPM header
//     // P6 = Binary RGB (fast and compact)
//     fprintf(fp, "P6\n%d %d\n255\n", width, height);

//     // Flip vertically while writing
//     for (int y = height - 1; y >= 0; y--) {
//         fwrite(pixels + y * width * 3, 3, width, fp);
//     }

//     fclose(fp);
//     free(pixels);

//     fprintf(stderr, "PPM: Wrote %s\n", filename);
//     sleep(2000);
// }


// /*
//  *
//  *  Draw a gear wheel.  You'll probably want to call this function when
//  *  building a display list since we do a lot of trig here.
//  * 
//  *  Input:  inner_radius - radius of hole at center
//  *          outer_radius - radius at center of teeth
//  *          width - width of gear
//  *          teeth - number of teeth
//  *          tooth_depth - depth of tooth
//  */
// static void
// gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
//      GLint teeth, GLfloat tooth_depth)
// {
//    GLint i;
//    GLfloat r0, r1, r2;
//    GLfloat angle, da;
//    GLfloat u, v, len;

//    r0 = inner_radius;
//    r1 = outer_radius - tooth_depth / 2.0;
//    r2 = outer_radius + tooth_depth / 2.0;

//    da = 2.0 * M_PI / teeth / 4.0;

//    glShadeModel(GL_FLAT);

//    glNormal3f(0.0, 0.0, 1.0);

//    /* draw front face */
//    glBegin(GL_QUAD_STRIP);
//    for (i = 0; i <= teeth; i++) {
//       angle = i * 2.0 * M_PI / teeth;
//       glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
//       glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
//       if (i < teeth) {
// 	 glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
// 	 glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
// 		    width * 0.5);
//       }
//    }
//    glEnd();

//    /* draw front sides of teeth */
//    glBegin(GL_QUADS);
//    da = 2.0 * M_PI / teeth / 4.0;
//    for (i = 0; i < teeth; i++) {
//       angle = i * 2.0 * M_PI / teeth;

//       glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
//       glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
//       glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
// 		 width * 0.5);
//       glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
// 		 width * 0.5);
//    }
//    glEnd();
//    GLenum error = glGetError();

// if (error != GL_NO_ERROR) {
//     fprintf(stderr, "[GL ERROR] glBegin/glEnd failed with code: 0x%x\n", error);
// } else {
//     fprintf(stderr, "[GL SUCCESS] glBegin/glEnd executed without error.\n");
// }

//    glNormal3f(0.0, 0.0, -1.0);

//    /* draw back face */
//    glBegin(GL_QUAD_STRIP);
//    for (i = 0; i <= teeth; i++) {
//       angle = i * 2.0 * M_PI / teeth;
//       glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
//       glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
//       if (i < teeth) {
// 	 glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
// 		    -width * 0.5);
// 	 glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
//       }
//    }
//    glEnd();

//    /* draw back sides of teeth */
//    glBegin(GL_QUADS);
//    da = 2.0 * M_PI / teeth / 4.0;
//    for (i = 0; i < teeth; i++) {
//       angle = i * 2.0 * M_PI / teeth;

//       glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
// 		 -width * 0.5);
//       glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
// 		 -width * 0.5);
//       glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
//       glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
//    }
//    glEnd();

//    /* draw outward faces of teeth */
//    glBegin(GL_QUAD_STRIP);
//    for (i = 0; i < teeth; i++) {
//       angle = i * 2.0 * M_PI / teeth;

//       glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
//       glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
//       u = r2 * cos(angle + da) - r1 * cos(angle);
//       v = r2 * sin(angle + da) - r1 * sin(angle);
//       len = sqrt(u * u + v * v);
//       u /= len;
//       v /= len;
//       glNormal3f(v, -u, 0.0);
//       glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
//       glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
//       glNormal3f(cos(angle), sin(angle), 0.0);
//       glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
// 		 width * 0.5);
//       glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
// 		 -width * 0.5);
//       u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
//       v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
//       glNormal3f(v, -u, 0.0);
//       glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
// 		 width * 0.5);
//       glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
// 		 -width * 0.5);
//       glNormal3f(cos(angle), sin(angle), 0.0);
//    }

//    glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
//    glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

//    glEnd();

//    glShadeModel(GL_SMOOTH);

//    /* draw inside radius cylinder */
//    glBegin(GL_QUAD_STRIP);
//    for (i = 0; i <= teeth; i++) {
//       angle = i * 2.0 * M_PI / teeth;
//       glNormal3f(-cos(angle), -sin(angle), 0.0);
//       glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
//       glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
//    }
//    glEnd();
// }


// static void
// draw(void)
// {
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//    glPushMatrix();
//    glRotatef(view_rotx, 1.0, 0.0, 0.0);
//    glRotatef(view_roty, 0.0, 1.0, 0.0);
//    glRotatef(view_rotz, 0.0, 0.0, 1.0);

//    glPushMatrix();
//    glTranslatef(-3.0, -2.0, 0.0);
//    glRotatef(angle, 0.0, 0.0, 1.0);
//    glCallList(gear1);
//    glPopMatrix();

//    glPushMatrix();
//    glTranslatef(3.1, -2.0, 0.0);
//    glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
//    glCallList(gear2);
//    glPopMatrix();

//    glPushMatrix();
//    glTranslatef(-3.1, 4.2, 0.0);
//    glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
//    glCallList(gear3);
//    glPopMatrix();

//    glPopMatrix();
// }


// static void
// draw_gears(void)
// {
//    if (stereo) {
//       /* First left eye.  */
//       glDrawBuffer(GL_BACK_LEFT);

//       glMatrixMode(GL_PROJECTION);
//       glLoadIdentity();
//       glFrustum(left, right, -asp, asp, 5.0, 60.0);

//       glMatrixMode(GL_MODELVIEW);

//       glPushMatrix();
//       glTranslated(+0.5 * eyesep, 0.0, 0.0);
//       draw();
//       glPopMatrix();

//       /* Then right eye.  */
//       glDrawBuffer(GL_BACK_RIGHT);

//       glMatrixMode(GL_PROJECTION);
//       glLoadIdentity();
//       glFrustum(-right, -left, -asp, asp, 5.0, 60.0);

//       glMatrixMode(GL_MODELVIEW);

//       glPushMatrix();
//       glTranslated(-0.5 * eyesep, 0.0, 0.0);
//       draw();
//       glPopMatrix();
//    }
//    else {
//       draw();
//    }
// }


// /** Draw single frame, do SwapBuffers, compute FPS */
// static void
// draw_frame(Display *dpy, Window win)
// {
//    static int frames = 0;
//    static double tRot0 = -1.0, tRate0 = -1.0;
//    double dt, t = current_time();

//    if (tRot0 < 0.0)
//       tRot0 = t;
//    dt = t - tRot0;
//    tRot0 = t;

//    if (animate) {
//       /* advance rotation for next frame */
//       angle += 70.0 * dt;  /* 70 degrees per second */
//       if (angle > 3600.0)
//          angle -= 3600.0;
//    }

//    draw_gears();
//    glFlush();
//    print_block();
//    dump_ppm("/host_tmp/schoooo.ppm", 1920, 1080);
//    glXSwapBuffers(dpy, win);

//    frames++;
   
//    if (tRate0 < 0.0)
//       tRate0 = t;
//    if (t - tRate0 >= 5.0) {
//       GLfloat seconds = t - tRate0;
//       GLfloat fps = frames / seconds;
//       printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds,
//              fps);
//       fflush(stdout);
//       tRate0 = t;
//       frames = 0;
//    }
// }

// /* new window size or exposure */
// static void
// reshape(int width, int height)
// {
//    glViewport(0, 0, (GLint) width, (GLint) height);

//    if (stereo) {
//       GLfloat w;

//       asp = (GLfloat) height / (GLfloat) width;
//       w = fix_point * (1.0 / 5.0);

//       left = -5.0 * ((w - 0.5 * eyesep) / fix_point);
//       right = 5.0 * ((w + 0.5 * eyesep) / fix_point);
//    }
//    else {
//       GLfloat h = (GLfloat) height / (GLfloat) width;

//       glMatrixMode(GL_PROJECTION);
//       glLoadIdentity();
//       glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
//    }
   
//    glMatrixMode(GL_MODELVIEW);
//    glLoadIdentity();
//    glTranslatef(0.0, 0.0, -40.0);
// }
   


// static void
// init(void)
// {
//    static GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
//    static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
//    static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
//    static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

//    glLightfv(GL_LIGHT0, GL_POSITION, pos);
//    glEnable(GL_CULL_FACE);
//    glEnable(GL_LIGHTING);
//    glEnable(GL_LIGHT0);
//    glEnable(GL_DEPTH_TEST);

//    /* make the gears */
//    gear1 = glGenLists(1);
//    glNewList(gear1, GL_COMPILE);
//    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
//    gear(1.0, 4.0, 1.0, 20, 0.7);
//    glEndList();

//    gear2 = glGenLists(1);
//    glNewList(gear2, GL_COMPILE);
//    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
//    gear(0.5, 2.0, 2.0, 10, 0.7);
//    glEndList();

//    gear3 = glGenLists(1);
//    glNewList(gear3, GL_COMPILE);
//    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
//    gear(1.3, 2.0, 0.5, 10, 0.7);
//    glEndList();

//    glEnable(GL_NORMALIZE);

//    fprintf(stderr, "Done init.\n");
//    // sleep(200000);
// }





// static void
// event_loop(Display *dpy, Window win)
// {
//    while (1) {
    

//       draw_frame(dpy, win);
//    }
// }
// static void print_gl_errors(const char *stage)
// {
//     GLenum e;
//     while ((e = glGetError()) != GL_NO_ERROR) {
//         fprintf(stderr, "[GLERR] %s: 0x%x\n", stage, e);
//     }
// }

// static void check_fbo_status(const char *tag)
// {
//     GLint fbo = 0;
//     glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
//     fprintf(stderr, "[FBO] %s: GL_FRAMEBUFFER_BINDING = %d\n", tag, fbo);

//     GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//     fprintf(stderr, "[FBO] %s: status = 0x%x\n", tag, status);

//     print_gl_errors("check_fbo_status-end");
// }
// static void dump_context_info(const char *tag)
// {
//     const GLubyte *ver   = glGetString(GL_VERSION);
//     const GLubyte *rend  = glGetString(GL_RENDERER);
//     const GLubyte *vend  = glGetString(GL_VENDOR);
//     const GLubyte *shver = glGetString(GL_SHADING_LANGUAGE_VERSION);

//     fprintf(stderr, "\n===== CONTEXT INFO (%s) =====\n", tag);
//     fprintf(stderr, "GL_VERSION   : %s\n", ver   ? (const char*)ver   : "(null)");
//     fprintf(stderr, "GL_RENDERER  : %s\n", rend  ? (const char*)rend  : "(null)");
//     fprintf(stderr, "GL_VENDOR    : %s\n", vend  ? (const char*)vend  : "(null)");
//     fprintf(stderr, "GLSL_VERSION : %s\n", shver ? (const char*)shver : "(null)");

//     GLint major = 0, minor = 0;
//     glGetIntegerv(GL_MAJOR_VERSION, &major);
//     glGetIntegerv(GL_MINOR_VERSION, &minor);
//     fprintf(stderr, "GL version   : %d.%d\n", major, minor);

// #ifdef GL_CONTEXT_PROFILE_MASK
//     GLint profile_mask = 0;
//     glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_mask);
//     fprintf(stderr, "Profile mask : 0x%x", profile_mask);
// #   ifdef GL_CONTEXT_CORE_PROFILE_BIT
//     if (profile_mask & GL_CONTEXT_CORE_PROFILE_BIT)
//         fprintf(stderr, " [CORE]");
// #   endif
// #   ifdef GL_CONTEXT_COMPATIBILITY_PROFILE_BIT
//     if (profile_mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
//         fprintf(stderr, " [COMPAT]");
// #   endif
//     fprintf(stderr, "\n");
// #endif

//     print_gl_errors("dump_context_info-end");
// }

// static void dump_full_state(const char *tag)
// {
//     fprintf(stderr, "\n================= STATE DUMP (%s) =================\n", tag);

//     // Context info
//     const GLubyte *ver   = glGetString(GL_VERSION);
//     const GLubyte *rend  = glGetString(GL_RENDERER);
//     const GLubyte *vend  = glGetString(GL_VENDOR);
//     const GLubyte *shver = glGetString(GL_SHADING_LANGUAGE_VERSION);

//     fprintf(stderr, "GL_VERSION   : %s\n", ver   ? (const char*)ver   : "(null)");
//     fprintf(stderr, "GL_RENDERER  : %s\n", rend  ? (const char*)rend  : "(null)");
//     fprintf(stderr, "GL_VENDOR    : %s\n", vend  ? (const char*)vend  : "(null)");
//     fprintf(stderr, "GLSL_VERSION : %s\n", shver ? (const char*)shver : "(null)");

//     GLint major = 0, minor = 0;
//     glGetIntegerv(GL_MAJOR_VERSION, &major);
//     glGetIntegerv(GL_MINOR_VERSION, &minor);
//     fprintf(stderr, "GL version   : %d.%d\n", major, minor);

// #ifdef GL_CONTEXT_PROFILE_MASK
//     GLint profile_mask = 0;
//     glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_mask);
//     fprintf(stderr, "Profile mask : 0x%x\n", profile_mask);
// #endif

//     // Program / FBO
//     GLint prog = 0;
//     glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
//     fprintf(stderr, "CURRENT_PROGRAM  : %d\n", prog);

//     GLint fbo = 0;
//     glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
//     fprintf(stderr, "FRAMEBUFFER_BINDING : %d\n", fbo);

//     GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//     fprintf(stderr, "FBO status      : 0x%x\n", fb_status);

//     // Viewport / scissor / polygon mode
//     GLint viewport[4];
//     glGetIntegerv(GL_VIEWPORT, viewport);
//     fprintf(stderr, "Viewport        : %d %d %d %d\n",
//             viewport[0], viewport[1], viewport[2], viewport[3]);

//     GLboolean scissor = GL_FALSE;
//     GLint sc_box[4];
//     glGetBooleanv(GL_SCISSOR_TEST, &scissor);
//     glGetIntegerv(GL_SCISSOR_BOX, sc_box);
//     fprintf(stderr, "Scissor         : enabled=%d box=%d %d %d %d\n",
//             scissor, sc_box[0], sc_box[1], sc_box[2], sc_box[3]);

//     GLint poly_mode[2];
//     glGetIntegerv(GL_POLYGON_MODE, poly_mode);
//     fprintf(stderr, "Polygon mode    : front=%d back=%d\n",
//             poly_mode[0], poly_mode[1]);

//     // Fixed-function bits that can wreck colors
//     GLboolean lighting = GL_FALSE, texturing = GL_FALSE;
//     glGetBooleanv(GL_LIGHTING, &lighting);
//     glGetBooleanv(GL_TEXTURE_2D, &texturing);
//     fprintf(stderr, "LIGHTING        : %d\n", lighting);
//     fprintf(stderr, "TEXTURE_2D      : %d\n", texturing);

//     GLint shade_model = 0;
//     glGetIntegerv(GL_SHADE_MODEL, &shade_model);
//     fprintf(stderr, "SHADE_MODEL     : %d\n", shade_model);

//     print_gl_errors("dump_full_state-end");
// }
// static void
// usage(void)
// {
//    printf("Usage:\n");
//    printf("  -display <displayname>  set the display to run on\n");
//    printf("  -stereo                 run in stereo mode\n");
//    printf("  -samples N              run in multisample mode with at least N samples\n");
//    printf("  -fullscreen             run in fullscreen mode\n");
//    printf("  -info                   display OpenGL renderer info\n");
//    printf("  -geometry WxH+X+Y       window geometry\n");
// }
 
// void print_block(){
//              GLubyte block[8*8*4]; // Buffer for 8x8 pixels (4 bytes/pixel)
// int cx = 1920/2;     // Center X coordinate
// int cy = 1080/2;    // Center Y coordinate
// glReadPixels(cx - 4, cy - 4, 8, 8, GL_RGB, GL_UNSIGNED_BYTE, block);
// GLenum e = glGetError();
//     if (e != GL_NO_ERROR) {
//         fprintf(stderr, "PPM: glReadPixels error: 0x%x\n", e);
//     }
// fprintf(stderr, "---- FBO TEST BLOCK 8x8 @ (%d,%d) ----\n", cx, cy);

// // The GL coordinate system typically places (0,0) at the bottom-left,
// // so we print from top to bottom (j=7 down to 0) to visualize it correctly.
// for (int j = 7; j >= 0; --j) {
//     for (int i = 0; i < 8; ++i) {
//         int idx = 4 * (j*8 + i);
//         // Print R G B A values in hexadecimal
//         fprintf(stderr, "(%02x %02x %02x %02x) ",
//         block[idx+0], block[idx+1], block[idx+2], block[idx+3]);
//     }
//     fprintf(stderr, "\n");
// }
// }
// static void dump_write_masks(const char *tag)
// {
//     GLboolean colorMask[4];
//     glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);

//     GLboolean depthMask;
//     glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);

//     GLint drawBuf;
//     glGetIntegerv(GL_DRAW_BUFFER, &drawBuf);

//     fprintf(stderr, "\n===== WRITE MASKS (%s) =====\n", tag);
//     fprintf(stderr, "ColorMask   : R=%d G=%d B=%d A=%d\n",
//             colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
//     fprintf(stderr, "DepthMask   : %d\n", depthMask);
//     fprintf(stderr, "DrawBuffer  : 0x%x\n", drawBuf);
// }

// int
// main(int argc, char *argv[])
// {
// //    unsigned int winWidth = 300, winHeight = 300;
// //    int x = 0, y = 0;
// //    Display *dpy;
// //    Window win;
// //    GLXContext ctx;
// //    char *dpyName = NULL;
// //    GLboolean printInfo = GL_FALSE;
// //    VisualID visId;
// //    int i;

// //    for (i = 1; i < argc; i++) {
// //       if (strcmp(argv[i], "-display") == 0) {
// //          dpyName = argv[i+1];
// //          i++;
// //       }
// //       else if (strcmp(argv[i], "-info") == 0) {
// //          printInfo = GL_TRUE;
// //       }
// //       else if (strcmp(argv[i], "-stereo") == 0) {
// //          stereo = GL_TRUE;
// //       }
// //       else if (i < argc-1 && strcmp(argv[i], "-samples") == 0) {
// //          samples = strtod(argv[i+1], NULL );
// //          ++i;
// //       }
// //       else if (strcmp(argv[i], "-fullscreen") == 0) {
// //          fullscreen = GL_TRUE;
// //       }
// //       else if (i < argc-1 && strcmp(argv[i], "-geometry") == 0) {
// //          XParseGeometry(argv[i+1], &x, &y, &winWidth, &winHeight);
// //          i++;
// //       }
// //       else {
// //          usage();
// //          return -1;
// //       }
// //    }

// //    dpy = XOpenDisplay(dpyName);
// //    if (!dpy) {
// //       printf("Error: couldn't open display %s\n",
// // 	     dpyName ? dpyName : getenv("DISPLAY"));
// //       return -1;
// //    }

// //    if (fullscreen) {
// //       int scrnum = DefaultScreen(dpy);

// //       x = 0; y = 0;
// //       winWidth = DisplayWidth(dpy, scrnum);
// //       winHeight = DisplayHeight(dpy, scrnum);
// //    }

// //    make_window(dpy, "glxgears", x, y, winWidth, winHeight, &win, &ctx, &visId);
// //    XMapWindow(dpy, win);
//    glXMakeCurrent(NULL, NULL, NULL);
//    dump_full_state("AFTER MAKE CURRENT");
// //    query_vsync(dpy, win);

   

//    // init();

//    /* Set initial projection/viewing transformation.
//     * We can't be sure we'll get a ConfigureNotify event when the window
//     * first appears.
//     */
//    // reshape(1920, 1080);

//    // event_loop(NULL, NULL);
//    // Inside glXMakeCurrent()
// // ... [FBO Binding Logic Here] ...
// // check_fbo_status("BEFORE DRAW");
// // glEnable(GL_DEPTH_TEST);
//    //  glClearDepth(1.0f);

// // for (int frame = 0; frame < 100000; ++frame) {
// //         int cur = frame % 2;
// //         glDisable(GL_DEPTH_TEST);
// // glDisable(GL_STENCIL_TEST);
// // glDisable(GL_SCISSOR_TEST);
// // glDisable(GL_CULL_FACE);
// // glDisable(GL_BLEND);

// // glViewport(0, 0, 1920, 1080);

// // glMatrixMode(GL_MODELVIEW);
// // glLoadIdentity();
// // glMatrixMode(GL_PROJECTION);
// // glLoadIdentity();

// // glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
// // glClear(GL_COLOR_BUFFER_BIT);

// // // *** simplest possible immediate-mode box ***
// // glColor3f(1, 0, 0);  // solid red

// // glBegin(GL_QUADS);
// //     glVertex2f(-0.5f, -0.5f);
// //     glVertex2f( 0.5f, -0.5f);
// //     glVertex2f( 0.5f,  0.5f);
// //     glVertex2f(-0.5f,  0.5f);
// // glEnd();
        
        
// //         /*
// //             [pid 124684] ioctl(5, DRM_IOCTL_SYNCOBJ_WAIT, 0x7ffc48fc78d0) = -1 ETIME (Timer expired)
// //             [pid 124684] ioctl(5, DRM_IOCTL_SYNCOBJ_WAIT, 0x7ffc48fc78d0) = 0  
// //         */
// //         // /* GPU sync - ensure GPU finished writing this buffer */
// //         glFlush();
// //         check_fbo_status("AFTER DRAW");
// //         print_block();
// //         dump_ppm("/host_tmp/schoooo.ppm", 1920, 1080);
// //         glXSwapBuffers(NULL, NULL);
// // }
// // // --- START glReadPixels TEST ---


// // glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
// // // Read an 8x8 block centered on the FBO
// // fprintf(stderr, "---------------------------------------\n");
// // glXSwapBuffers(NULL, NULL);

// // --- END glReadPixels TEST ---
// // ... [Run glReadPixels test here] ...
// //    glDeleteLists(gear1, 1);
// //    glDeleteLists(gear2, 1);
// //    glDeleteLists(gear3, 1);
// // //    glXMakeCurrent(dpy, None, NULL);
// //    glXDestroyContext(dpy, ctx);
// //    XDestroyWindow(dpy, win);
// //    XCloseDisplay(dpy);

//    return 0;
// }

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#ifndef GLX_MESA_swap_control
#define GLX_MESA_swap_control 1
typedef int (*PFNGLXGETSWAPINTERVALMESAPROC)(void);
#endif


#define BENCHMARK

#ifdef BENCHMARK

/* XXX this probably isn't very portable */

#include <sys/time.h>
#include <unistd.h>

/* return current time (in seconds) */
static double
current_time(void)
{
   struct timeval tv;
#ifdef __VMS
   (void) gettimeofday(&tv, NULL );
#else
   struct timezone tz;
   (void) gettimeofday(&tv, &tz);
#endif
   return (double) tv.tv_sec + tv.tv_usec / 1000000.0;
}

#else /*BENCHMARK*/

/* dummy */
static double
current_time(void)
{
   /* update this function for other platforms! */
   static double t = 0.0;
   static int warn = 1;
   if (warn) {
      fprintf(stderr, "Warning: current_time() not implemented!!\n");
      warn = 0;
   }
   return t += 1.0;
}

#endif /*BENCHMARK*/



#ifndef M_PI
#define M_PI 3.14159265
#endif


/** Event handler results: */
#define NOP 0
#define EXIT 1
#define DRAW 2

static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static GLboolean fullscreen = GL_FALSE;	/* Create a single fullscreen window */
static GLboolean stereo = GL_FALSE;	/* Enable stereo.  */
static GLint samples = 0;               /* Choose visual with at least N samples. */
static GLboolean animate = GL_TRUE;	/* Animation */
static GLfloat eyesep = 5.0;		/* Eye separation. */
static GLfloat fix_point = 40.0;	/* Fixation point distance.  */
static GLfloat left, right, asp;	/* Stereo frustum params.  */


/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 * 
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth)
{
   GLint i;
   GLfloat r0, r1, r2;
   GLfloat angle, da;
   GLfloat u, v, len;

   r0 = inner_radius;
   r1 = outer_radius - tooth_depth / 2.0;
   r2 = outer_radius + tooth_depth / 2.0;

   da = 2.0 * M_PI / teeth / 4.0;

   glShadeModel(GL_FLAT);

   glNormal3f(0.0, 0.0, 1.0);

   /* draw front face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      if (i < teeth) {
	 glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
	 glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		    width * 0.5);
      }
   }
   glEnd();

   /* draw front sides of teeth */
   glBegin(GL_QUADS);
   da = 2.0 * M_PI / teeth / 4.0;
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
		 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		 width * 0.5);
   }
   glEnd();
   GLenum error = glGetError();

if (error != GL_NO_ERROR) {
    fprintf(stderr, "[GL ERROR] glBegin/glEnd failed with code: 0x%x\n", error);
} else {
    fprintf(stderr, "[GL SUCCESS] glBegin/glEnd executed without error.\n");
}

   glNormal3f(0.0, 0.0, -1.0);

   /* draw back face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      if (i < teeth) {
	 glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		    -width * 0.5);
	 glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      }
   }
   glEnd();

   /* draw back sides of teeth */
   glBegin(GL_QUADS);
   da = 2.0 * M_PI / teeth / 4.0;
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		 -width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
		 -width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
   }
   glEnd();

   /* draw outward faces of teeth */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      u = r2 * cos(angle + da) - r1 * cos(angle);
      v = r2 * sin(angle + da) - r1 * sin(angle);
      len = sqrt(u * u + v * v);
      u /= len;
      v /= len;
      glNormal3f(v, -u, 0.0);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glNormal3f(cos(angle), sin(angle), 0.0);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
		 width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
		 -width * 0.5);
      u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
      v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
      glNormal3f(v, -u, 0.0);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		 -width * 0.5);
      glNormal3f(cos(angle), sin(angle), 0.0);
   }

   glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
   glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

   glEnd();

   glShadeModel(GL_SMOOTH);

   /* draw inside radius cylinder */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glNormal3f(-cos(angle), -sin(angle), 0.0);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
   }
   glEnd();
}


static void
draw(void)
{
      static GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
   static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
   static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
   static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glPushMatrix();
   glRotatef(view_rotx, 1.0, 0.0, 0.0);
   glRotatef(view_roty, 0.0, 1.0, 0.0);
   glRotatef(view_rotz, 0.0, 0.0, 1.0);



   glPushMatrix();
   glTranslatef(-3.0, -2.0, 0.0);
   glRotatef(angle, 0.0, 0.0, 1.0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
   gear(1.0, 4.0, 1.0, 20, 0.7);
   glPopMatrix();

   glPushMatrix();
   glTranslatef(3.1, -2.0, 0.0);
   glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
   gear(0.5, 2.0, 2.0, 10, 0.7);
   glPopMatrix();

   glPushMatrix();
   glTranslatef(-3.1, 4.2, 0.0);
   glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
   gear(1.3, 2.0, 0.5, 10, 0.7);
   glPopMatrix();

   glPopMatrix();
}

/**
 * Remove window border/decorations.
 */
static void
no_border( Display *dpy, Window w)
{
   static const unsigned MWM_HINTS_DECORATIONS = (1 << 1);
   static const int PROP_MOTIF_WM_HINTS_ELEMENTS = 5;

   typedef struct
   {
      unsigned long       flags;
      unsigned long       functions;
      unsigned long       decorations;
      long                inputMode;
      unsigned long       status;
   } PropMotifWmHints;

   PropMotifWmHints motif_hints;
   Atom prop, proptype;
   unsigned long flags = 0;

   /* setup the property */
   motif_hints.flags = MWM_HINTS_DECORATIONS;
   motif_hints.decorations = flags;

   /* get the atom for the property */
   prop = XInternAtom( dpy, "_MOTIF_WM_HINTS", True );
   if (!prop) {
      /* something went wrong! */
      return;
   }

   /* not sure this is correct, seems to work, XA_WM_HINTS didn't work */
   proptype = prop;

   XChangeProperty( dpy, w,                         /* display, window */
                    prop, proptype,                 /* property, type */
                    32,                             /* format: 32-bit datums */
                    PropModeReplace,                /* mode */
                    (unsigned char *) &motif_hints, /* data */
                    PROP_MOTIF_WM_HINTS_ELEMENTS    /* nelements */
                  );
}

/*
 * Create an RGB, double-buffered window.
 * Return the window and context handles.
 */
static void
make_window( Display *dpy, const char *name,
             int x, int y, int width, int height,
             Window *winRet, GLXContext *ctxRet, VisualID *visRet)
{

   int attribs[64];
   int i = 0;

   int scrnum;
   XSetWindowAttributes attr;
   unsigned long mask;
   Window root;
   Window win;
   GLXContext ctx;
   XVisualInfo *visinfo;

   /* Singleton attributes. */
   attribs[i++] = GLX_RGBA;
   attribs[i++] = GLX_DOUBLEBUFFER;
   if (stereo)
      attribs[i++] = GLX_STEREO;

   /* Key/value attributes. */
   attribs[i++] = GLX_RED_SIZE;
   attribs[i++] = 1;
   attribs[i++] = GLX_GREEN_SIZE;
   attribs[i++] = 1;
   attribs[i++] = GLX_BLUE_SIZE;
   attribs[i++] = 1;
   attribs[i++] = GLX_DEPTH_SIZE;
   attribs[i++] = 1;
   if (samples > 0) {
      attribs[i++] = GLX_SAMPLE_BUFFERS;
      attribs[i++] = 1;
      attribs[i++] = GLX_SAMPLES;
      attribs[i++] = samples;
   }

   attribs[i++] = None;
      fprintf(stderr, "Hey da. \n");
   scrnum = DefaultScreen( dpy );
   root = RootWindow( dpy, scrnum );
   fprintf(stderr, "Hey da. \n");
int maj, min;
// glXQueryVersion(dpy, &maj, &min);
// printf("GLX version: %d.%d\n", maj, min);
   visinfo = glXChooseVisual(dpy, scrnum, attribs);

   if (!visinfo) {
      printf("Error: couldn't get an RGB, Double-buffered");
      if (stereo)
         printf(", Stereo");
      if (samples > 0)
         printf(", Multisample");
      printf(" visual\n");
      exit(1);
   }

   /* window attributes */
   attr.background_pixel = 0;
   attr.border_pixel = 0;
      fprintf(stderr, "Hey da. \n");

   attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);
      fprintf(stderr, "Hey da. \n");

   attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
   /* XXX this is a bad way to get a borderless window! */
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   win = XCreateWindow( dpy, root, x, y, width, height,
		        0, visinfo->depth, InputOutput,
		        visinfo->visual, mask, &attr );

   if (fullscreen)
      no_border(dpy, win);

   /* set hints and properties */
   {
      XSizeHints sizehints;
      sizehints.x = x;
      sizehints.y = y;
      sizehints.width  = width;
      sizehints.height = height;
      sizehints.flags = USSize | USPosition;
      XSetNormalHints(dpy, win, &sizehints);
      XSetStandardProperties(dpy, win, name, name,
                              None, (char **)NULL, 0, &sizehints);
   }

   ctx = glXCreateContext( dpy, visinfo, NULL, True );
   if (!ctx) {
      printf("Error: glXCreateContext failed\n");
      exit(1);
   }

   *winRet = win;
   *ctxRet = ctx;
   *visRet = visinfo->visualid;

   XFree(visinfo);
}
static void
draw_gears(void)
{
   if (stereo) {
      /* First left eye.  */
      glDrawBuffer(GL_BACK_LEFT);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glFrustum(left, right, -asp, asp, 5.0, 60.0);

      glMatrixMode(GL_MODELVIEW);

      glPushMatrix();
      glTranslated(+0.5 * eyesep, 0.0, 0.0);
      draw();
      glPopMatrix();

      /* Then right eye.  */
      glDrawBuffer(GL_BACK_RIGHT);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glFrustum(-right, -left, -asp, asp, 5.0, 60.0);

      glMatrixMode(GL_MODELVIEW);

      glPushMatrix();
      glTranslated(-0.5 * eyesep, 0.0, 0.0);
      draw();
      glPopMatrix();
   }
   else {
      draw();
   }
}


/** Draw single frame, do SwapBuffers, compute FPS */
static void
draw_frame(Display *dpy, Window win)
{
   static int frames = 0;
   static double tRot0 = -1.0, tRate0 = -1.0;
   double dt, t = current_time();

   if (tRot0 < 0.0)
      tRot0 = t;
   dt = t - tRot0;
   tRot0 = t;

   if (animate) {
      /* advance rotation for next frame */
      angle += 70.0 * dt;  /* 70 degrees per second */
      if (angle > 3600.0)
         angle -= 3600.0;
   }

   draw_gears();
   glXSwapBuffers(dpy, win);

   frames++;
   
   if (tRate0 < 0.0)
      tRate0 = t;
   if (t - tRate0 >= 5.0) {
      GLfloat seconds = t - tRate0;
      GLfloat fps = frames / seconds;
      printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds,
             fps);
      fflush(stdout);
      tRate0 = t;
      frames = 0;
   }
}
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

/* new window size or exposure */
static void
reshape(int width, int height)
{
   glViewport(0, 0, (GLint) width, (GLint) height);

   if (stereo) {
      GLfloat w;

      asp = (GLfloat) height / (GLfloat) width;
      w = fix_point * (1.0 / 5.0);

      left = -5.0 * ((w - 0.5 * eyesep) / fix_point);
      right = 5.0 * ((w + 0.5 * eyesep) / fix_point);
   }
   else {
      GLfloat h = (GLfloat) height / (GLfloat) width;

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
   }
   
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef(0.0, 0.0, -40.0);
}
   


static void
init(void)
{
   static GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
   static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
   static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
   static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

   glLightfv(GL_LIGHT0, GL_POSITION, pos);
   glEnable(GL_CULL_FACE);
   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);
   glEnable(GL_DEPTH_TEST);

   // /* make the gears */
   // gear1 = glGenLists(1);
   // glNewList(gear1, GL_COMPILE);
   // glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
   // gear(1.0, 4.0, 1.0, 20, 0.7);
   // glEndList();

   // gear2 = glGenLists(1);
   // glNewList(gear2, GL_COMPILE);
   // glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
   // gear(0.5, 2.0, 2.0, 10, 0.7);
   // glEndList();

   // gear3 = glGenLists(1);
   // glNewList(gear3, GL_COMPILE);
   // glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
   // gear(1.3, 2.0, 0.5, 10, 0.7);
   // glEndList();

   glEnable(GL_NORMALIZE);

   printf("Done init.\n");
}





static void
event_loop(Display *dpy, Window win)
{
   while (1) {
    

      draw_frame(dpy, win);
   }
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
   unsigned int winWidth = 1920, winHeight = 1080;
   int x = 0, y = 0;
   Display *dpy;
   Window win;
   GLXContext ctx;
   char *dpyName = NULL;
//    GLboolean printInfo = GL_FALSE;
   VisualID visId;
//    int i;

//    for (i = 1; i < argc; i++) {
//       if (strcmp(argv[i], "-display") == 0) {
//          dpyName = argv[i+1];
//          i++;
//       }
//       else if (strcmp(argv[i], "-info") == 0) {
//          printInfo = GL_TRUE;
//       }
//       else if (strcmp(argv[i], "-stereo") == 0) {
//          stereo = GL_TRUE;
//       }
//       else if (i < argc-1 && strcmp(argv[i], "-samples") == 0) {
//          samples = strtod(argv[i+1], NULL );
//          ++i;
//       }
//       else if (strcmp(argv[i], "-fullscreen") == 0) {
//          fullscreen = GL_TRUE;
//       }
//       else if (i < argc-1 && strcmp(argv[i], "-geometry") == 0) {
//          XParseGeometry(argv[i+1], &x, &y, &winWidth, &winHeight);
//          i++;
//       }
//       else {
//          usage();
//          return -1;
//       }
//    }

   dpy = XOpenDisplay(NULL);
   if (!dpy) {
      printf("Error: couldn't open display %s\n",
	     dpyName ? dpyName : getenv("DISPLAY"));
      return -1;
   }

// //    if (fullscreen) {
// //       int scrnum = DefaultScreen(dpy);

// //       x = 0; y = 0;
// //       winWidth = DisplayWidth(dpy, scrnum);
// //       winHeight = DisplayHeight(dpy, scrnum);
// //    }
__sync_synchronize();
fprintf(stderr,"What ra??\n");
   make_window(dpy, "glxgears", x, y, winWidth, winHeight, &win, &ctx, &visId);
//    XMapWindow(dpy, win);
   glXMakeCurrent(NULL, NULL, NULL);
   dump_full_state("AFTER MAKE CURRENT");
//    query_vsync(dpy, win);

   

   init();

   /* Set initial projection/viewing transformation.
    * We can't be sure we'll get a ConfigureNotify event when the window
    * first appears.
    */
   reshape(1920, 1080);

   event_loop(NULL, NULL);
   // Inside glXMakeCurrent()
// ... [FBO Binding Logic Here] ...
// check_fbo_status("BEFORE DRAW");
// GLuint quad_list = glGenLists(1);
// glViewport(0, 0, 1920, 1080);

// glNewList(quad_list, GL_COMPILE);
//     glBegin(GL_QUADS);
//         glVertex2f(-0.5f, -0.5f);
//         glVertex2f( 0.5f, -0.5f);
//         glVertex2f( 0.5f,  0.5f);
//         glVertex2f(-0.5f,  0.5f);
//     glEnd();
// glEndList();

// // glEnable(GL_DEPTH_TEST);
//    //  glClearDepth(1.0f);

// for (int frame = 0; frame < 100000; ++frame) {
//         int cur = frame % 2;
//         glDisable(GL_DEPTH_TEST);
// glDisable(GL_STENCIL_TEST);
// glDisable(GL_SCISSOR_TEST);
// glDisable(GL_CULL_FACE);
// glDisable(GL_BLEND);
// glViewport(0, 0, 1920, 1080);


// glMatrixMode(GL_MODELVIEW);
// glLoadIdentity();
// glMatrixMode(GL_PROJECTION);
// glLoadIdentity();

// glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
// glClear(GL_COLOR_BUFFER_BIT);

// // *** simplest possible immediate-mode box ***
// glColor3f(1, 0, 0);  // solid red

//    //  glBegin(GL_QUADS);
//    //      glVertex2f(-0.5f, -0.5f);
//    //      glVertex2f( 0.5f, -0.5f);
//    //      glVertex2f( 0.5f,  0.5f);
//    //      glVertex2f(-0.5f,  0.5f);
//    //  glEnd();        
//         glCallList(quad_list);
//         /*
//             [pid 124684] ioctl(5, DRM_IOCTL_SYNCOBJ_WAIT, 0x7ffc48fc78d0) = -1 ETIME (Timer expired)
//             [pid 124684] ioctl(5, DRM_IOCTL_SYNCOBJ_WAIT, 0x7ffc48fc78d0) = 0  
//         */
//         // /* GPU sync - ensure GPU finished writing this buffer */
//         glFlush();
//         check_fbo_status("AFTER DRAW");
//         print_block();
//         dump_ppm("/host_tmp/schoooo.ppm", 1920, 1080);
//         glXSwapBuffers(NULL, NULL);
//         return 0;


// }
// --- START glReadPixels TEST ---


// glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
// Read an 8x8 block centered on the FBO
// fprintf(stderr, "---------------------------------------\n");
// glXSwapBuffers(NULL, NULL);

// --- END glReadPixels TEST ---
// ... [Run glReadPixels test here] ...
//    glDeleteLists(gear1, 1);
//    glDeleteLists(gear2, 1);
//    glDeleteLists(gear3, 1);
// //    glXMakeCurrent(dpy, None, NULL);
//    glXDestroyContext(dpy, ctx);
//    XDestroyWindow(dpy, win);
//    XCloseDisplay(dpy);

   return 0;
}

