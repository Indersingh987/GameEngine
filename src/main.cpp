#include <SDL2/SDL.h>
#include <iostream>
#include "Entity.h"
#include "AudioManager.h"

int main(int argc, char* argv[]) {
    // 1. Initialize SDL's video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // 2. Create a window
    SDL_Window* window = SDL_CreateWindow(
        "Game Engine",              // title
        SDL_WINDOWPOS_CENTERED,     // x position
        SDL_WINDOWPOS_CENTERED,     // y position
        800, 600,                   // width, height
        SDL_WINDOW_SHOWN
    );

    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create a renderer tied to this window
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,                          // let SDL pick the best driver
        SDL_RENDERER_ACCELERATED     // use GPU acceleration if available
    );

    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    //initiale AudioManager
    AudioManager audio;
    if (!audio.init()) {
        std::cerr << "Audio initialization failed!" << std::endl;
    }

    // Example loading — replace paths with actual files you have
    bool loaded = audio.loadSound("jump", "assets/jump.ogg");
    std::cerr << "Sound loaded: " << (loaded ? "YES" : "NO") << std::endl;
    
    // 3. The game loop

    // Example: a gravity-affected player, and a non-gravity obstacle
    Entity player(350.0f, 250.0f, 100, 100, 300.0f, 0.0f); // gravity = 980 → platformer style
    Entity obstacle(100.0f, 100.0f, 80, 80, 0.0f, 0.0f);     // gravity = 0 → static



    bool running = true;
    SDL_Event event;

     Uint64 lastTime = SDL_GetTicks64(); // timestamp before the loop starts

    while (running) {

        Uint64 currentTime = SDL_GetTicks64();
        float deltaTime = (currentTime - lastTime) / 1000.0f; // convert milliseconds to seconds
        lastTime = currentTime;

        // --- process input/events ---
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                player.jump();
                audio.playSound("jump");
            }
        }

        // --- check continuous key state ---
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);

        player.handleInput(keystate); // Handle input for the player entity
        player.update(deltaTime); // Update the player entity's position based on input and deltaTime

        bool colliding = checkCollision(player, obstacle);
        if (colliding) {
            player.resolveCollision(obstacle);
        }

        // --- render (nothing drawn yet, just an empty window) ---
        SDL_SetRenderDrawColor(renderer, 30, 30, 60, 255);  // Set background color
        SDL_RenderClear(renderer);                        // Clear the screen

        player.render(renderer); // Render the player entity 
        obstacle.render(renderer); // Render the obstacle entity
        SDL_RenderPresent(renderer);// Present the rendered frame
    }

    // 4. Cleanup
    audio.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}