#include <X11/Xlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    int thread_id;
    int success;
    char error_msg[256];
} thread_data_t;

void* x11_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    Display* display;
    int screen;
    Window window;

    printf("Thread %d: Starting X11 connection...\n", data->thread_id);

    // Connect to X server
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        snprintf(data->error_msg, sizeof(data->error_msg), "Thread %d: Cannot open display",
                 data->thread_id);
        data->success = 0;
        return NULL;
    }

    printf("Thread %d: Connected to X11 server\n", data->thread_id);

    screen = DefaultScreen(display);

    // Create a simple window
    window = XCreateSimpleWindow(display, RootWindow(display, screen), 10 + data->thread_id * 50,
                                 10 + data->thread_id * 50, 200, 150, 1,
                                 BlackPixel(display, screen), WhitePixel(display, screen));

    if (window) {
        printf("Thread %d: Window created\n", data->thread_id);
        XMapWindow(display, window);
        XFlush(display);

        // Sleep briefly to simulate work
        sleep(5);

        XDestroyWindow(display, window);
        printf("Thread %d: Window destroyed\n", data->thread_id);
    } else {
        snprintf(data->error_msg, sizeof(data->error_msg), "Thread %d: Failed to create window",
                 data->thread_id);
        data->success = 0;
        XCloseDisplay(display);
        return NULL;
    }

    // Query some display info
    int width  = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);
    printf("Thread %d: Display size: %dx%d\n", data->thread_id, width, height);

    // Close connection
    XCloseDisplay(display);
    printf("Thread %d: X11 connection closed\n", data->thread_id);

    data->success = 1;
    return NULL;
}

int main() {
    const int num_threads = 6;
    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];

    printf("Main: Creating %d threads for X11 connections...\n", num_threads);

    // Create multiple threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id    = i;
        thread_data[i].success      = 0;
        thread_data[i].error_msg[0] = '\0';
        // x11_thread(&thread_data[i]);  // Run synchronously
        // sleep(2);

        int result = pthread_create(&threads[i], NULL, x11_thread, &thread_data[i]);
        if (result != 0) {
            printf("Main: Failed to create thread %d\n", i);
            return 1;
        }
        printf("Main: Thread %d created\n", i);
    }

    // Wait for all threads to complete
    printf("Main: Waiting for threads to complete...\n");
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        if (thread_data[i].success) {
            printf("Main: Thread %d completed successfully\n", i);
        } else {
            printf("Main: Thread %d failed: %s\n", i, thread_data[i].error_msg);
        }
    }

    printf("Main: All threads completed\n");
    return 0;
}
