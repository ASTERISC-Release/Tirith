// sdl_input_test.c
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("Hello, SDL Input Test!\n");

    SDL_Window *window = SDL_CreateWindow(
        "SDL Input Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Ensure window is visible */
    SDL_ShowWindow(window);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;
    printf("Press ESC or close the window to quit.\n");

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    printf("Key down: %s (scancode %d)\n",
                           SDL_GetKeyName(event.key.keysym.sym),
                           event.key.keysym.scancode);
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                    break;
                case SDL_KEYUP:
                    printf("Key up: %s\n", SDL_GetKeyName(event.key.keysym.sym));
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    printf("Mouse button %d down at (%d, %d)\n",
                           event.button.button, event.button.x, event.button.y);
                    break;
                case SDL_MOUSEBUTTONUP:
                    printf("Mouse button %d up at (%d, %d)\n",
                           event.button.button, event.button.x, event.button.y);
                    break;
                case SDL_MOUSEMOTION:
                    /* comment this out if it spams too much */
                    printf("Mouse moved to (%d, %d)\n", event.motion.x, event.motion.y);
                    break;
                default:
                    break;
            }
        }

        /* Draw a simple background so the window is visibly updating */
        SDL_SetRenderDrawColor(renderer, 32, 96, 160, 255); // slightly bluish (RGB)
        SDL_RenderClear(renderer);

        /* present */
        SDL_RenderPresent(renderer);

        /* small delay to avoid 100% CPU */
        SDL_Delay(10);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

