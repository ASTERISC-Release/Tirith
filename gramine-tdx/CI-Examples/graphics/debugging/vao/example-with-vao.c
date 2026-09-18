#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>

// --- Shader Sources ---
const char* vertex_shader_src = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "out vec3 ourColor;\n"
    "void main() {\n"
    "   gl_Position = vec4(aPos, 1.0);\n"
    "   ourColor = aColor;\n"
    "}\0";

const char* fragment_shader_src = 
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec3 ourColor;\n"
    "void main() {\n"
    "   FragColor = vec4(ourColor, 1.0f);\n"
    "}\n\0";

int main() {
    // 1. Keep your working X11 initialization
    Display *dpy = XOpenDisplay(NULL);
    int attr[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 1920, 1080, 0, 
                               vi->depth, InputOutput, vi->visual, CWColormap, &swa);
    XMapWindow(dpy, win);
    GLXContext ctx = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, ctx);

    // 2. Compile Shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex_shader_src, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragment_shader_src, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // 3. Define Geometry (Positions + Colors)
    float vertices[] = {
        // positions         // colors
         0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // Top (Red)
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // Bottom Left (Green)
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // Bottom Right (Blue)
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 4. Render Loop
    while(1) {
        glViewport(0, 0, 1920, 1080);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glXSwapBuffers(dpy, win);
    }
    return 0;
}