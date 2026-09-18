#ifndef _SGL_CONTEXT_H_
#define _SGL_CONTEXT_H_

#include <SDL2/SDL.h>

struct sgl_host_context {
    SDL_Window *window;
    SDL_GLContext gl_context;
    int client_id; // Adil: To identify different clients.
};

void sgl_set_max_resolution(int width, int height);
void sgl_get_max_resolution(int *width, int *height);

struct sgl_host_context *sgl_context_create();
void sgl_context_destroy(struct sgl_host_context *ctx);
void sgl_set_current(struct sgl_host_context *ctx);
void *sgl_read_pixels(unsigned int width, unsigned int height, void *data, int vflip, int format, size_t mem_usage);

// Adil: Read pixels and directly display on host SDL window.
void *sgl_read_pixels_host(unsigned int width, unsigned int height, void *data, 
        int vflip, int format, size_t mem_usage, struct sgl_host_context *ctx);

#endif
