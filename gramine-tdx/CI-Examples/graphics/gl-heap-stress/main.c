#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <SDL2/SDL.h>

#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum phase {
    PHASE_CONTEXT = 1 << 0,
    PHASE_HEAP = 1 << 1,
    PHASE_SHADER = 1 << 2,
    PHASE_BUFFER = 1 << 3,
    PHASE_RENDER = 1 << 4,
    PHASE_ALL = (1 << 5) - 1,
};

struct options {
    unsigned phases;
    unsigned cycles;
    unsigned shader_iterations;
    unsigned buffer_iterations;
    unsigned frames;
    unsigned workers;
    bool sockets;
};

struct worker_state {
    atomic_bool stop;
    unsigned index;
};

static void symbol_routing_probe(void)
{
    struct symbol_check {
        const char *name;
        void *incompatible_wrapper;
    } checks[] = {
        {"eglMakeCurrent", (void *)glXMakeCurrent},
        {"eglSwapBuffers", (void *)glXSwapBuffers},
        {"glXMakeContextCurrent", (void *)glXMakeCurrent},
        {"glXCreateContextAttribsARB", (void *)glXCreateContext},
    };

    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        void *resolved = dlsym(RTLD_DEFAULT, checks[i].name);
        if (!resolved) {
            fprintf(stderr, "dlsym failed for %s: %s\n", checks[i].name, dlerror());
            abort();
        }
        if (resolved == checks[i].incompatible_wrapper) {
            fprintf(stderr, "%s was routed to an ABI-incompatible GLX wrapper\n",
                    checks[i].name);
            abort();
        }
    }
    fprintf(stderr, "dynamic symbol routing probe passed\n");
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--phase context|heap|shader|buffer|render|all] "
            "[--cycles N] [--shader-iterations N] [--buffer-iterations N] "
            "[--frames N] [--workers N] [--no-sockets]\n",
            program);
}

static unsigned parse_uint(const char *text, const char *name)
{
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);

    if (!text[0] || !end || *end || value > UINT32_MAX) {
        fprintf(stderr, "invalid %s: %s\n", name, text);
        exit(2);
    }
    return (unsigned)value;
}

static unsigned parse_phase(const char *text)
{
    if (!strcmp(text, "context"))
        return PHASE_CONTEXT;
    if (!strcmp(text, "heap"))
        return PHASE_CONTEXT | PHASE_HEAP;
    if (!strcmp(text, "shader"))
        return PHASE_CONTEXT | PHASE_HEAP | PHASE_SHADER;
    if (!strcmp(text, "buffer"))
        return PHASE_CONTEXT | PHASE_HEAP | PHASE_BUFFER;
    if (!strcmp(text, "render"))
        return PHASE_ALL;
    if (!strcmp(text, "all"))
        return PHASE_ALL;

    fprintf(stderr, "unknown phase: %s\n", text);
    exit(2);
}

static struct options parse_options(int argc, char **argv)
{
    struct options options = {
        .phases = PHASE_ALL,
        .cycles = 20,
        .shader_iterations = 256,
        .buffer_iterations = 256,
        .frames = 240,
        .workers = 0,
        .sockets = true,
    };

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--phase") && i + 1 < argc)
            options.phases = parse_phase(argv[++i]);
        else if (!strcmp(argv[i], "--cycles") && i + 1 < argc)
            options.cycles = parse_uint(argv[++i], "cycle count");
        else if (!strcmp(argv[i], "--shader-iterations") && i + 1 < argc)
            options.shader_iterations = parse_uint(argv[++i], "shader iteration count");
        else if (!strcmp(argv[i], "--buffer-iterations") && i + 1 < argc)
            options.buffer_iterations = parse_uint(argv[++i], "buffer iteration count");
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            options.frames = parse_uint(argv[++i], "frame count");
        else if (!strcmp(argv[i], "--workers") && i + 1 < argc)
            options.workers = parse_uint(argv[++i], "worker count");
        else if (!strcmp(argv[i], "--no-sockets"))
            options.sockets = false;
        else {
            usage(argv[0]);
            exit(2);
        }
    }
    return options;
}

static int open_udp_socket(void)
{
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(0),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0 || bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        fprintf(stderr, "UDP socket setup failed: %s\n", strerror(errno));
        if (fd >= 0)
            close(fd);
        return -1;
    }
    return fd;
}

static void heap_probe(unsigned salt)
{
    enum { BLOCKS = 2048 };
    void *blocks[BLOCKS];
    size_t sizes[BLOCKS];

    for (unsigned i = 0; i < BLOCKS; i++) {
        sizes[i] = 17 + ((i * 131u + salt * 977u) % 32749u);
        blocks[i] = malloc(sizes[i]);
        if (!blocks[i]) {
            fprintf(stderr, "malloc failed in heap probe\n");
            exit(1);
        }
        memset(blocks[i], (unsigned char)(i ^ salt), sizes[i]);
    }
    for (unsigned pass = 0; pass < 2; pass++) {
        for (unsigned i = pass; i < BLOCKS; i += 2) {
            unsigned char expected = (unsigned char)(i ^ salt);
            unsigned char *bytes = blocks[i];
            if (bytes[0] != expected || bytes[sizes[i] - 1] != expected) {
                fprintf(stderr, "heap payload changed at block %u\n", i);
                abort();
            }
            free(blocks[i]);
        }
    }
}

static void *heap_worker(void *argument)
{
    struct worker_state *state = argument;
    uint32_t value = 0x9e3779b9u ^ state->index;

    while (!atomic_load_explicit(&state->stop, memory_order_relaxed)) {
        enum { BATCH = 128 };
        void *blocks[BATCH];
        size_t sizes[BATCH];

        for (unsigned i = 0; i < BATCH; i++) {
            value = value * 1664525u + 1013904223u;
            sizes[i] = 8 + value % 65529u;
            blocks[i] = malloc(sizes[i]);
            if (!blocks[i])
                abort();
            memset(blocks[i], (unsigned char)value, sizes[i]);
        }
        for (unsigned i = 0; i < BATCH; i++)
            free(blocks[(i * 73u) % BATCH]);
        /* The VM currently has one vCPU. Keep allocator activity concurrent
         * without starving the render thread under that scheduler. */
        usleep(1000);
    }
    return NULL;
}

static GLuint compile_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint ok = GL_FALSE;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "shader compile failed: %s\n", log);
        exit(1);
    }
    return shader;
}

static void shader_stress(unsigned iterations)
{
    static const char vertex_source[] =
        "#version 120\n"
        "attribute vec2 position;\n"
        "void main() { gl_Position = vec4(position, 0.0, 1.0); }\n";
    static const char fragment_source[] =
        "#version 120\n"
        "void main() { gl_FragColor = vec4(0.2, 0.5, 0.8, 1.0); }\n";

    for (unsigned i = 0; i < iterations; i++) {
        GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
        GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
        GLuint program = glCreateProgram();
        GLint ok = GL_FALSE;

        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glBindAttribLocation(program, 0, "position");
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(program, sizeof(log), NULL, log);
            fprintf(stderr, "program link failed: %s\n", log);
            exit(1);
        }
        glDeleteProgram(program);
        glDeleteShader(fragment);
        glDeleteShader(vertex);
    }
    glFinish();
}

static void buffer_stress(unsigned iterations)
{
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);

    for (unsigned i = 0; i < iterations; i++) {
        size_t size = 4096 + (i % 32) * 4096;
        glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STREAM_DRAW);
        unsigned char *data = glMapBufferRange(GL_ARRAY_BUFFER, 0, size,
                                                GL_MAP_WRITE_BIT |
                                                GL_MAP_INVALIDATE_BUFFER_BIT);
        if (!data) {
            fprintf(stderr, "glMapBufferRange failed at iteration %u (GL error %#x)\n",
                    i, glGetError());
            exit(1);
        }
        memset(data, (unsigned char)i, size);
        if (glUnmapBuffer(GL_ARRAY_BUFFER) != GL_TRUE) {
            fprintf(stderr, "mapped buffer contents became invalid\n");
            exit(1);
        }
    }
    glFinish();
    glDeleteBuffers(1, &buffer);
}

static int run_cycle(const struct options *options, unsigned cycle)
{
    SDL_Window *window = NULL;
    SDL_GLContext context = NULL;
    int result = 1;

    fprintf(stderr, "[cycle %u] context begin\n", cycle);
    window = SDL_CreateWindow("GL heap stress",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto out;
    }
    context = SDL_GL_CreateContext(window);
    if (!context) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        goto out;
    }
    SDL_GL_SetSwapInterval(0);
    fprintf(stderr, "[cycle %u] context ready: %s / %s\n", cycle,
            glGetString(GL_RENDERER), glGetString(GL_VERSION));

    if (options->phases & PHASE_HEAP) {
        fprintf(stderr, "[cycle %u] heap begin\n", cycle);
        heap_probe(cycle);
        fprintf(stderr, "[cycle %u] heap done\n", cycle);
    }
    if (options->phases & PHASE_SHADER) {
        fprintf(stderr, "[cycle %u] shaders begin\n", cycle);
        shader_stress(options->shader_iterations);
        heap_probe(cycle + 0x1000);
        fprintf(stderr, "[cycle %u] shaders done\n", cycle);
    }
    if (options->phases & PHASE_BUFFER) {
        fprintf(stderr, "[cycle %u] buffers begin\n", cycle);
        buffer_stress(options->buffer_iterations);
        heap_probe(cycle + 0x2000);
        fprintf(stderr, "[cycle %u] buffers done\n", cycle);
    }
    if (options->phases & PHASE_RENDER) {
        fprintf(stderr, "[cycle %u] presentation begin\n", cycle);
        for (unsigned i = 0; i < options->frames; i++) {
            float shade = (float)(i % 120) / 119.0f;
            glClearColor(shade, 0.15f, 1.0f - shade, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            SDL_GL_SwapWindow(window);
            SDL_PumpEvents();
        }
        glFinish();
        heap_probe(cycle + 0x3000);
        fprintf(stderr, "[cycle %u] presentation done\n", cycle);
    }
    result = 0;

out:
    if (context)
        SDL_GL_DeleteContext(context);
    if (window)
        SDL_DestroyWindow(window);
    fprintf(stderr, "[cycle %u] teardown done\n", cycle);
    return result;
}

int main(int argc, char **argv)
{
    struct options options = parse_options(argc, argv);
    int sockets[2] = {-1, -1};
    pthread_t *worker_threads = NULL;
    struct worker_state *workers = NULL;
    int result = 1;

    setvbuf(stderr, NULL, _IONBF, 0);
    symbol_routing_probe();
    if (options.sockets) {
        sockets[0] = open_udp_socket();
        sockets[1] = open_udp_socket();
        if (sockets[0] < 0 || sockets[1] < 0)
            goto out;
        fprintf(stderr, "two UDP sockets ready\n");
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        goto out;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if (options.workers) {
        worker_threads = calloc(options.workers, sizeof(*worker_threads));
        workers = calloc(options.workers, sizeof(*workers));
        if (!worker_threads || !workers)
            abort();
        for (unsigned i = 0; i < options.workers; i++) {
            workers[i].index = i;
            atomic_init(&workers[i].stop, false);
            if (pthread_create(&worker_threads[i], NULL, heap_worker, &workers[i]) != 0) {
                fprintf(stderr, "failed to create heap worker %u\n", i);
                abort();
            }
        }
        fprintf(stderr, "%u concurrent heap workers ready\n", options.workers);
    }

    for (unsigned cycle = 0; cycle < options.cycles; cycle++) {
        if (run_cycle(&options, cycle) != 0)
            goto out_sdl;
    }
    fprintf(stderr, "PASS: completed %u cycle(s)\n", options.cycles);
    result = 0;

out_sdl:
    for (unsigned i = 0; i < options.workers; i++)
        atomic_store_explicit(&workers[i].stop, true, memory_order_relaxed);
    for (unsigned i = 0; i < options.workers; i++)
        pthread_join(worker_threads[i], NULL);
    free(workers);
    free(worker_threads);
    SDL_Quit();
out:
    if (sockets[0] >= 0)
        close(sockets[0]);
    if (sockets[1] >= 0)
        close(sockets[1]);
    return result;
}
