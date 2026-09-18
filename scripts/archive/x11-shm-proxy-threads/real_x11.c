#define _GNU_SOURCE
#include "real_x11.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>

/* statics for function pointers */
static XOpenDisplay_fn_t real_XOpenDisplay = NULL;
static XCloseDisplay_fn_t real_XCloseDisplay = NULL;
static XCreateWindow_fn_t real_XCreateWindow = NULL;
static XMapWindow_fn_t real_XMapWindow = NULL;
static XDestroyWindow_fn_t real_XDestroyWindow = NULL;
static XInternAtom_fn_t real_XInternAtom = NULL;
static XFlush_fn_t real_XFlush = NULL;
static XSync_fn_t real_XSync = NULL;
static XGetWindowAttributes_fn_t real_XGetWindowAttributes = NULL;
static XLockDisplay_fn_t real_XLockDisplay = NULL;
static XUnlockDisplay_fn_t real_XUnlockDisplay = NULL;
static XDefaultScreen_fn_t real_XDefaultScreen = NULL;

static pthread_once_t loader_once = PTHREAD_ONCE_INIT;

/* helper: try dlsym with RTLD_NEXT then candidate libs then RTLD_DEFAULT */
static void *resolve_symbol(const char *sym, const char *candidates[]) {
#ifdef RTLD_NEXT
    void *p = dlsym(RTLD_NEXT, sym);
    if (p) return p;
#endif

    for (const char **c = candidates; *c; ++c) {
        void *h = dlopen(*c, RTLD_LAZY | RTLD_LOCAL);
        if (!h) continue;
        dlerror();
        p = dlsym(h, sym);
        if (p) return p;
        /* keep handle open intentionally */
    }

    return dlsym(RTLD_DEFAULT, sym);
}

static void loader(void) {
    const char *cands[] = {"libX11.so.6", "libX11.so", NULL};
    real_XOpenDisplay = (XOpenDisplay_fn_t)resolve_symbol("XOpenDisplay", cands);
    real_XCloseDisplay = (XCloseDisplay_fn_t)resolve_symbol("XCloseDisplay", cands);
    real_XCreateWindow = (XCreateWindow_fn_t)resolve_symbol("XCreateWindow", cands);
    real_XMapWindow = (XMapWindow_fn_t)resolve_symbol("XMapWindow", cands);
    real_XDestroyWindow = (XDestroyWindow_fn_t)resolve_symbol("XDestroyWindow", cands);
    real_XInternAtom = (XInternAtom_fn_t)resolve_symbol("XInternAtom", cands);
    real_XFlush = (XFlush_fn_t)resolve_symbol("XFlush", cands);
    real_XSync = (XSync_fn_t)resolve_symbol("XSync", cands);
    real_XGetWindowAttributes = (XGetWindowAttributes_fn_t)resolve_symbol("XGetWindowAttributes", cands);
    real_XLockDisplay = (XLockDisplay_fn_t)resolve_symbol("XLockDisplay", cands);
    real_XUnlockDisplay = (XUnlockDisplay_fn_t)resolve_symbol("XUnlockDisplay", cands);
    real_XDefaultScreen = (XDefaultScreen_fn_t)resolve_symbol("XDefaultScreen", cands);
}

XOpenDisplay_fn_t get_real_XOpenDisplay(void) {
    pthread_once(&loader_once, loader);
    return real_XOpenDisplay;
}
XCloseDisplay_fn_t get_real_XCloseDisplay(void) {
    pthread_once(&loader_once, loader);
    return real_XCloseDisplay;
}
XCreateWindow_fn_t get_real_XCreateWindow(void) {
    pthread_once(&loader_once, loader);
    return real_XCreateWindow;
}
XMapWindow_fn_t get_real_XMapWindow(void) {
    pthread_once(&loader_once, loader);
    return real_XMapWindow;
}
XDestroyWindow_fn_t get_real_XDestroyWindow(void) {
    pthread_once(&loader_once, loader);
    return real_XDestroyWindow;
}
XInternAtom_fn_t get_real_XInternAtom(void) {
    pthread_once(&loader_once, loader);
    return real_XInternAtom;
}
XFlush_fn_t get_real_XFlush(void) {
    pthread_once(&loader_once, loader);
    return real_XFlush;
}
XSync_fn_t get_real_XSync(void) {
    pthread_once(&loader_once, loader);
    return real_XSync;
}
XGetWindowAttributes_fn_t get_real_XGetWindowAttributes(void) {
    pthread_once(&loader_once, loader);
    return real_XGetWindowAttributes;
}
XLockDisplay_fn_t get_real_XLockDisplay(void) {
    pthread_once(&loader_once, loader);
    return real_XLockDisplay;
}
XUnlockDisplay_fn_t get_real_XUnlockDisplay(void) {
    pthread_once(&loader_once, loader);
    return real_XUnlockDisplay;
}

XDefaultScreen_fn_t get_real_XDefaultScreen(void) {
    pthread_once(&loader_once, loader);
    return real_XDefaultScreen;
}

