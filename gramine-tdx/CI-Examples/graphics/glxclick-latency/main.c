#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "input_ring.h"

#define DEFAULT_CLICK_COUNT 100U
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define SG_COMM_ADDR UINT64_C(0xf00000)

static sg_input_latency_t *input_latency_control(void) {
    sg_input_latency_t *control = (sg_input_latency_t *)(uintptr_t)
        (SG_COMM_ADDR + SG_INPUT_LATENCY_OFFSET);

    if (__atomic_load_n(&control->magic, __ATOMIC_ACQUIRE) !=
            SG_INPUT_LATENCY_MAGIC ||
            __atomic_load_n(&control->enabled, __ATOMIC_ACQUIRE) != 1)
        return NULL;
    return control;
}

static uint64_t read_tsc(void) {
    uint32_t low;
    uint32_t high;

    __asm__ volatile("lfence; rdtscp; lfence"
                     : "=a"(low), "=d"(high) :: "rcx", "memory");
    return ((uint64_t)high << 32) | low;
}

static uint64_t monotonic_raw_ns(void) {
    struct timespec timestamp;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &timestamp) != 0)
        return 0;
    return (uint64_t)timestamp.tv_sec * UINT64_C(1000000000) +
           (uint64_t)timestamp.tv_nsec;
}

static unsigned int parse_click_count(int argc, char **argv) {
    char *end = NULL;
    unsigned long value;

    if (argc == 1)
        return DEFAULT_CLICK_COUNT;
    if (argc != 2) {
        fprintf(stderr, "usage: %s [click-count]\n", argv[0]);
        exit(2);
    }

    errno = 0;
    value = strtoul(argv[1], &end, 10);
    if (errno || !end || *end != '\0' || value == 0 || value > UINT_MAX) {
        fprintf(stderr, "invalid click count: %s\n", argv[1]);
        exit(2);
    }
    return (unsigned int)value;
}

static void draw(Display *display, Window window, unsigned int click_index) {
    static const GLfloat colors[][3] = {
        {0.08f, 0.12f, 0.20f},
        {0.12f, 0.55f, 0.20f},
    };
    const GLfloat *color = colors[click_index & 1U];

    glClearColor(color[0], color[1], color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glXSwapBuffers(display, window);
}

int main(int argc, char **argv) {
    const unsigned int target_clicks = parse_click_count(argc, argv);
    const char *native_environment = getenv("NATIVE_INPUT_LATENCY");
    const bool native_mode = native_environment &&
        strcmp(native_environment, "1") == 0;
    int visual_attributes[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        None,
    };
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "could not open X display %s\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
        return 1;
    }

    const int screen = DefaultScreen(display);
    XVisualInfo *visual = glXChooseVisual(display, screen, visual_attributes);
    if (!visual) {
        fprintf(stderr, "could not find a double-buffered GLX visual\n");
        XCloseDisplay(display);
        return 1;
    }

    XSetWindowAttributes attributes = {
        .colormap = XCreateColormap(display, RootWindow(display, screen),
                                    visual->visual, AllocNone),
        .event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask,
    };
    Window window = XCreateWindow(display, RootWindow(display, screen),
                                  0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                                  visual->depth, InputOutput, visual->visual,
                                  CWColormap | CWEventMask, &attributes);
    XStoreName(display, window,
               native_mode ? "Native Click Latency" : "Gramine Click Latency");

    GLXContext context = glXCreateContext(display, visual, NULL, True);
    XFree(visual);
    if (!context) {
        fprintf(stderr, "could not create GLX context\n");
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return 1;
    }

    XMapRaised(display, window);
    XFlush(display);
    if (!glXMakeCurrent(display, window, context)) {
        fprintf(stderr, "could not make GLX context current\n");
        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return 1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    draw(display, window, 0);
    sg_input_latency_t *latency = native_mode ? NULL : input_latency_control();
    if (!native_mode && !latency) {
        fprintf(stderr, "input latency control is not enabled; use run-experiment.sh\n");
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return 1;
    }
    printf("click_latency_ready mode=%s count=%u\n",
           native_mode ? "native" : "gramine", target_clicks);
    fflush(stdout);

    unsigned int clicks = 0;
    while (clicks < target_clicks) {
        /* Games normally poll their X event queue once per frame. Keep this
         * minimal workload hot so the result measures transport latency rather
         * than an arbitrary sleep or frame-rate cap. */
        if (XPending(display) == 0)
            continue;

        XEvent event;
        XNextEvent(display, &event);
        if (event.type != ButtonPress || event.xbutton.button != Button1)
            continue;

        uint64_t native_delivery_ns = native_mode ? monotonic_raw_ns() : 0;

        /* This state transition is the test workload's input action: it is
         * analogous to a game spawning a projectile from a button handler.
         * Acknowledge only after applying it, then render the new state. */
        clicks++;
        if (native_mode) {
            uint64_t action_ns = monotonic_raw_ns();
            printf("native_input_timing delivery_ns=%" PRIu64
                   " action_ns=%" PRIu64 " index=%u\n",
                   native_delivery_ns, action_ns, clicks - 1);
        } else {
            uint64_t action_tsc = read_tsc();
            __atomic_store_n(&latency->action_tsc, action_tsc, __ATOMIC_RELAXED);
            __atomic_store_n(&latency->processed_sequence, clicks, __ATOMIC_RELEASE);
            printf("guest_click_processed guest_tsc=%" PRIu64 " index=%u\n",
                   action_tsc, clicks - 1);
        }
        fflush(stdout);
        draw(display, window, clicks);
    }

    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
