#include <SDL2/SDL.h>
#include <stdio.h>

int main() {
    printf("1. Starting SDL test...\n");
    
    printf("2. Calling SDL_Init...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("3. SDL_Init successful\n");
    
    printf("4. Creating window...\n");
    SDL_Window* window = SDL_CreateWindow("Test", 
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        640, 480,
                                        SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("5. Window created\n");
    
    printf("6. Creating renderer...\n");
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 
                                              SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("7. Renderer created\n");
    
    printf("8. Cleaning up...\n");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("9. Cleanup complete\n");
    
    return 0;
}
