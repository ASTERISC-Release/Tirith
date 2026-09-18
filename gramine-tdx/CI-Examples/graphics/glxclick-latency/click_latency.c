#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_CLICK_COUNT 100U
#define DEFAULT_CLICK_INTERVAL_MS 50U
#define DEVICE_SETTLE_MS 750U

static long long monotonic_raw_ns(void) {
    struct timespec timestamp;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &timestamp) != 0) {
        fprintf(stderr, "clock_gettime failed: %s\n", strerror(errno));
        exit(1);
    }
    return (long long)timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
}

static unsigned int parse_positive(const char *text, const char *name) {
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno || !end || *end != '\0' || value == 0 || value > UINT_MAX) {
        fprintf(stderr, "invalid %s: %s\n", name, text);
        exit(2);
    }
    return (unsigned int)value;
}

static void checked_ioctl_arg(int fd, unsigned long request, int argument,
                              const char *name) {
    if (ioctl(fd, request, argument) < 0) {
        fprintf(stderr, "%s failed: %s\n", name, strerror(errno));
        exit(1);
    }
}

static void emit_event(int fd, unsigned short type, unsigned short code, int value) {
    struct input_event event = {
        .type = type,
        .code = code,
        .value = value,
    };

    if (write(fd, &event, sizeof(event)) != (ssize_t)sizeof(event)) {
        fprintf(stderr, "write input event failed: %s\n", strerror(errno));
        exit(1);
    }
}

static int create_mouse(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "open /dev/uinput failed: %s\n", strerror(errno));
        exit(1);
    }

    checked_ioctl_arg(fd, UI_SET_EVBIT, EV_KEY, "UI_SET_EVBIT EV_KEY");
    checked_ioctl_arg(fd, UI_SET_KEYBIT, BTN_LEFT, "UI_SET_KEYBIT BTN_LEFT");
    checked_ioctl_arg(fd, UI_SET_EVBIT, EV_REL, "UI_SET_EVBIT EV_REL");
    checked_ioctl_arg(fd, UI_SET_RELBIT, REL_X, "UI_SET_RELBIT REL_X");
    checked_ioctl_arg(fd, UI_SET_RELBIT, REL_Y, "UI_SET_RELBIT REL_Y");
    checked_ioctl_arg(fd, UI_SET_PROPBIT, INPUT_PROP_POINTER,
                      "UI_SET_PROPBIT INPUT_PROP_POINTER");

    struct uinput_setup setup = {0};
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "gramine-click-latency");
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1;
    setup.id.product = 0x1;
    setup.id.version = 1;
    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        fprintf(stderr, "could not create uinput mouse: %s\n", strerror(errno));
        exit(1);
    }
    return fd;
}

static void sleep_ms(unsigned int milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000U,
        .tv_nsec = (long)(milliseconds % 1000U) * 1000000L,
    };

    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

int main(int argc, char **argv) {
    unsigned int click_count = DEFAULT_CLICK_COUNT;
    unsigned int interval_ms = DEFAULT_CLICK_INTERVAL_MS;

    if (argc > 3) {
        fprintf(stderr, "usage: %s [click-count] [interval-ms]\n", argv[0]);
        return 2;
    }
    if (argc >= 2)
        click_count = parse_positive(argv[1], "click count");
    if (argc == 3)
        interval_ms = parse_positive(argv[2], "click interval");

    int fd = create_mouse();
    sleep_ms(DEVICE_SETTLE_MS);

    /* Ensure compositors which activate a newly hot-plugged pointer on its
     * first relative motion associate subsequent buttons with the seat's
     * cursor. Return to the original coordinate before measuring. */
    emit_event(fd, EV_REL, REL_X, 1);
    emit_event(fd, EV_SYN, SYN_REPORT, 0);
    emit_event(fd, EV_REL, REL_X, -1);
    emit_event(fd, EV_SYN, SYN_REPORT, 0);
    sleep_ms(25);

    for (unsigned int index = 0; index < click_count; index++) {
        long long click_ns = monotonic_raw_ns();
        emit_event(fd, EV_KEY, BTN_LEFT, 1);
        emit_event(fd, EV_SYN, SYN_REPORT, 0);
        emit_event(fd, EV_KEY, BTN_LEFT, 0);
        emit_event(fd, EV_SYN, SYN_REPORT, 0);

        printf("click_inject_left_press_ns=%lld index=%u\n", click_ns, index);
        fflush(stdout);
        if (index + 1 < click_count)
            sleep_ms(interval_ms);
    }

    /* Keep the virtual device alive long enough for the host input stack and
     * Xwayland to drain the final press/release pair. Destroying it
     * immediately can discard the last event after it has been timestamped. */
    sleep_ms(interval_ms);

    if (ioctl(fd, UI_DEV_DESTROY) < 0)
        fprintf(stderr, "warning: UI_DEV_DESTROY failed: %s\n", strerror(errno));
    close(fd);
    return 0;
}
