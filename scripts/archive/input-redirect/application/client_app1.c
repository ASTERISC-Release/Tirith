#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "CLIENT WINDOW (input should come from server)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    bool running = true;
    SDL_Event ev;

    printf("Client started. Press keys or click in SERVER window.\n");

    while (running) {
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                printf("[CLIENT] SDL_QUIT\n");
                running = false;
                break;

            case SDL_KEYDOWN:
                printf("[CLIENT] KEYDOWN  sym=%s (%d)\n",
                       SDL_GetKeyName(ev.key.keysym.sym),
                       ev.key.keysym.sym);
                if (ev.key.keysym.sym == SDLK_ESCAPE)
                    running = false;
                break;

            case SDL_KEYUP:
                printf("[CLIENT] KEYUP    sym=%s (%d)\n",
                       SDL_GetKeyName(ev.key.keysym.sym),
                       ev.key.keysym.sym);
                break;

            case SDL_MOUSEBUTTONDOWN:
                printf("[CLIENT] MOUSE DOWN button=%d x=%d y=%d\n",
                       ev.button.button, ev.button.x, ev.button.y);
                break;

            case SDL_MOUSEMOTION:
                printf("[CLIENT] MOUSE MOVE x=%d y=%d\n",
                       ev.motion.x, ev.motion.y);
                break;

            default:
                printf("[CLIENT] Event type=%d\n", ev.type);
                break;
            }
        }

        SDL_Delay(16); // ~60 FPS loop
    }

    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

