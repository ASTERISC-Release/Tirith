
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define DEFAULT_STATS_INTERVAL 5000

typedef void (*glXSwapBuffers_t)(Display *, GLXDrawable);
typedef void (*SDL_GL_SwapWindow_t)(void *);
typedef EGLBoolean (*eglSwapBuffers_t)(EGLDisplay, EGLSurface);
typedef void *(*SDL_GL_GetProcAddress_t)(const char *);
typedef __GLXextFuncPtr (*glXGetProcAddressARB_t)(const GLubyte *);
typedef __GLXextFuncPtr (*glXGetProcAddress_t)(const GLubyte *);
typedef __eglMustCastToProperFunctionPointerType (*eglGetProcAddress_t)(const char *);

static glXSwapBuffers_t real_glXSwapBuffers = NULL;
static SDL_GL_SwapWindow_t real_SDL_GL_SwapWindow = NULL;
static eglSwapBuffers_t real_eglSwapBuffers = NULL;
static SDL_GL_GetProcAddress_t real_SDL_GL_GetProcAddress = NULL;
static glXGetProcAddressARB_t real_glXGetProcAddressARB = NULL;
static glXGetProcAddress_t real_glXGetProcAddress = NULL;
static eglGetProcAddress_t real_eglGetProcAddress = NULL;
static pthread_mutex_t latency_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t cpu_latency_sum = 0;
static uint64_t cpu_latency_count = 0;
static struct timespec prev_swap_end = {0, 0};
static uint64_t total_swaps = 0;
static uint64_t stats_interval = DEFAULT_STATS_INTERVAL;

static int init_done = 0;

static void init_once(void) {
    if (init_done)
        return;

    real_glXSwapBuffers = dlsym(RTLD_NEXT, "glXSwapBuffers");
    real_SDL_GL_SwapWindow = dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
    real_eglSwapBuffers = dlsym(RTLD_NEXT, "eglSwapBuffers");
    real_SDL_GL_GetProcAddress = dlsym(RTLD_NEXT, "SDL_GL_GetProcAddress");
    real_glXGetProcAddressARB = dlsym(RTLD_NEXT, "glXGetProcAddressARB");
    real_glXGetProcAddress = dlsym(RTLD_NEXT, "glXGetProcAddress");
    real_eglGetProcAddress = dlsym(RTLD_NEXT, "eglGetProcAddress");

    const char *interval_env = getenv("LATENCY_STATS_INTERVAL");
    if (interval_env && interval_env[0]) {
        char *end = NULL;
        unsigned long long parsed = strtoull(interval_env, &end, 10);
        if (end && *end == '\0' && parsed > 0)
            stats_interval = parsed;
    }

    fprintf(stderr,
            "[latency] loaded: glXSwapBuffers=%s SDL_GL_SwapWindow=%s eglSwapBuffers=%s interval=%lu\n",
            real_glXSwapBuffers ? "yes" : "no",
            real_SDL_GL_SwapWindow ? "yes" : "no",
            real_eglSwapBuffers ? "yes" : "no",
            stats_interval);
    init_done = 1;
}

__attribute__((constructor))
static void latency_preload_loaded(void) {
    static pthread_once_t init_control = PTHREAD_ONCE_INIT;
    pthread_once(&init_control, init_once);
}

static uint64_t timespec_delta_ns(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
           (end.tv_nsec - start.tv_nsec);
}

static void record_cpu_gap(struct timespec now) {
    if (prev_swap_end.tv_sec == 0 && prev_swap_end.tv_nsec == 0)
        return;

    uint64_t cpu_ns = timespec_delta_ns(prev_swap_end, now);
    pthread_mutex_lock(&latency_mutex);
    cpu_latency_sum += cpu_ns;
    cpu_latency_count++;
    if (cpu_latency_count >= stats_interval) {
        double avg_cpu_ms = (cpu_latency_sum / cpu_latency_count) / 1000000.0;
        fprintf(stderr, "[cpu_latency] Avg over %lu frames: %.2f ms\n",
                cpu_latency_count, avg_cpu_ms);
        cpu_latency_sum = 0;
        cpu_latency_count = 0;
    }
    pthread_mutex_unlock(&latency_mutex);
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
    static pthread_once_t init_control = PTHREAD_ONCE_INIT;
    pthread_once(&init_control, init_once);

    if (!real_glXSwapBuffers) {
        fprintf(stderr, "[latency] real glXSwapBuffers is unavailable\n");
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    record_cpu_gap(now);

    glFlush();
    real_glXSwapBuffers(dpy, drawable);
    XSync(dpy, False);

    struct timespec cpu_end;
    clock_gettime(CLOCK_MONOTONIC, &cpu_end);
    prev_swap_end = cpu_end;

    // record_latencies("glX", render_ns, display_ns, display_end);
}

void SDL_GL_SwapWindow(void *window) {
    static pthread_once_t init_control = PTHREAD_ONCE_INIT;
    pthread_once(&init_control, init_once);

    if (!real_SDL_GL_SwapWindow) {
        fprintf(stderr, "[latency] real SDL_GL_SwapWindow is unavailable\n");
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    record_cpu_gap(now);

    glFlush();
    real_SDL_GL_SwapWindow(window);

    struct timespec cpu_end;
    clock_gettime(CLOCK_MONOTONIC, &cpu_end);
    prev_swap_end = cpu_end;

}

EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    static pthread_once_t init_control = PTHREAD_ONCE_INIT;
    pthread_once(&init_control, init_once);

    if (!real_eglSwapBuffers) {
        fprintf(stderr, "[latency] real eglSwapBuffers is unavailable\n");
        return EGL_FALSE;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    record_cpu_gap(now);

    glFlush();

    EGLBoolean ret = real_eglSwapBuffers(display, surface);
    struct timespec cpu_end;
    clock_gettime(CLOCK_MONOTONIC, &cpu_end);
    prev_swap_end = cpu_end;

  
    return ret;
}

void *SDL_GL_GetProcAddress(const char *proc) {
    static pthread_once_t init_control = PTHREAD_ONCE_INIT;
    pthread_once(&init_control, init_once);

    if (proc && strcmp(proc, "glXSwapBuffers") == 0)
        return (void *)glXSwapBuffers;
    if (proc && strcmp(proc, "eglSwapBuffers") == 0)
        return (void *)eglSwapBuffers;

    return real_SDL_GL_GetProcAddress ? real_SDL_GL_GetProcAddress(proc) : NULL;
}

__GLXextFuncPtr glXGetProcAddressARB(const GLubyte *proc) {
    static pthread_once_t init_control = PTHREAD_ONCE_INIT;
    pthread_once(&init_control, init_once);

    if (proc && strcmp((const char *)proc, "glXSwapBuffers") == 0)
        return (__GLXextFuncPtr)glXSwapBuffers;

    return real_glXGetProcAddressARB ? real_glXGetProcAddressARB(proc) : NULL;
}

__GLXextFuncPtr glXGetProcAddress(const GLubyte *proc) {
    static pthread_once_t init_control = PTHREAD_ONCE_INIT;
    pthread_once(&init_control, init_once);

    if (proc && strcmp((const char *)proc, "glXSwapBuffers") == 0)
        return (__GLXextFuncPtr)glXSwapBuffers;

    return real_glXGetProcAddress ? real_glXGetProcAddress(proc) : NULL;
}

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *proc) {
    static pthread_once_t init_control = PTHREAD_ONCE_INIT;
    pthread_once(&init_control, init_once);

    if (proc && strcmp(proc, "eglSwapBuffers") == 0)
        return (__eglMustCastToProperFunctionPointerType)eglSwapBuffers;

    return real_eglGetProcAddress ? real_eglGetProcAddress(proc) : NULL;
}
