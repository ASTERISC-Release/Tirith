#include <client/glimpl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifndef _WIN32
#include <client/platform/glx.h>
#endif

/* Adil: track whether we should redirect to the host SDL window */
bool host_sdl_window = false;

void __attribute__((constructor)) sharedgl_entry(void) 
{
    char *host_sdl_window_ptr = getenv("HOST_SDL_WINDOW"); // Adil: Gramine sets this env var

    /* Adil: If HOST_SDL_WINDOW is defined, mark it as such for backend files */
    if (host_sdl_window_ptr == NULL) {
      fprintf(stderr, "Using GUEST SDL window for display\n");
      host_sdl_window = false;
    } else {
      fprintf(stderr, "Using HOST SDL window for display\n");
      host_sdl_window = true;
    }

    glimpl_init();
#ifndef _WIN32
    glximpl_init();
#endif

}

void __attribute__((destructor)) sharedgl_goodbye(void) 
{
    glimpl_goodbye();
}
