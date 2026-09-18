#define GL_GLEXT_PROTOTYPES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

static const char *vsrc =
"#version 330 core\n"
"layout(location=0) in vec3 pos;"
"layout(location=1) in vec3 col;"
"out vec3 vcol;"
"uniform mat4 mvp;"
"void main(){ vcol=col; gl_Position=mvp*vec4(pos,1.0); }";

static const char *fsrc =
"#version 330 core\n"
"in vec3 vcol;"
"out vec4 FragColor;"
"void main(){ FragColor=vec4(vcol,1.0); }";

static GLuint prog, vao;
static float angle = 0.0f;

static void compile(GLenum type, const char *src, GLuint *out)
{
    *out = glCreateShader(type);
    glShaderSource(*out, 1, &src, NULL);
    glCompileShader(*out);
    GLint ok;
    glGetShaderiv(*out, GL_COMPILE_STATUS, &ok);
    if(!ok){ char log[512]; glGetShaderInfoLog(*out,512,NULL,log); fprintf(stderr,"%s\n",log); exit(1);}
}

static void init_scene(void)
{
    GLuint vs, fs;
    compile(GL_VERTEX_SHADER, vsrc, &vs);
    compile(GL_FRAGMENT_SHADER, fsrc, &fs);
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(prog);

    GLfloat verts[] = {
        // pos             // col
        -0.7f,-0.7f,0,    1,0,0,
         0.7f,-0.7f,0,    0,1,0,
         0.0f, 0.7f,0,    0,0,1
    };

    GLuint vbo;
    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);
    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
}

static void draw(Display *dpy, Window win)
{
    glClearColor(0.1,0.1,0.1,1);
    glClear(GL_COLOR_BUFFER_BIT);
    angle += 0.01f;

    float c=cosf(angle), s=sinf(angle);
    float mvp[16] = {
         c,-s,0,0,
         s, c,0,0,
         0, 0,1,0,
         0, 0,0,1
    };
    GLint loc = glGetUniformLocation(prog,"mvp");
    glUniformMatrix4fv(loc,1,GL_FALSE,mvp);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES,0,3);
    glXSwapBuffers(dpy, win);
}

int main(void)
{
    Display *dpy = XOpenDisplay(NULL);
    if(!dpy){ fprintf(stderr,"Cannot open X display\n"); return 1; }

    int fbcount;
    static int fbattrs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, True, None
    };

    GLXFBConfig *fbc = glXChooseFBConfig(dpy, DefaultScreen(dpy), fbattrs, &fbcount);
    if(!fbc){ fprintf(stderr,"No FBConfig\n"); return 1; }
    XVisualInfo *vi = glXGetVisualFromFBConfig(dpy, fbc[0]);

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    swa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen),
                               0,0,1920,1080,0, vi->depth, InputOutput,
                               vi->visual, CWColormap | CWEventMask, &swa);
    XStoreName(dpy, win, "GLX Gears (Core 3.3)");
    XMapWindow(dpy, win);

    // --- Modern context request ---
    static int ctx_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };
    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB =
        (void*)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    GLXContext ctx = glXCreateContextAttribsARB(dpy, fbc[0], 0, True, ctx_attribs);
    if(!ctx){ fprintf(stderr,"Core context creation failed.\n"); return 1; }
    glXMakeCurrent(dpy, win, ctx);

    init_scene();

    XEvent ev;
    while(1){
        while(XPending(dpy)){
            XNextEvent(dpy,&ev);
            if(ev.type==KeyPress) goto end;
        }
        draw(dpy, win);
    }
end:
    glXMakeCurrent(dpy,None,NULL);
    glXDestroyContext(dpy,ctx);
    XDestroyWindow(dpy,win);
    XCloseDisplay(dpy);
    return 0;
}
