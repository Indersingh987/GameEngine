#include <SDL2/SDL.h>
#include <iostream>
#include "Engine/AudioManager.h"
#include "Game/GameplayScene.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <string>

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Game Engine",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    AudioManager audio;
    if (!audio.init()) {
        std::cerr << "Audio initialization failed!" << std::endl;
    }
    audio.loadSound("jump", "assets/jump.ogg");

   GameplayScene gameplayScene(audio);

    bool running = true;
    SDL_Event event;
    Uint64 lastTime = SDL_GetTicks64();

    while (running) {
        Uint64 currentTime = SDL_GetTicks64();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                audio.playSound("jump");
            }
        }

        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        gameplayScene.handleInput(keystate);
        gameplayScene.update(deltaTime);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        static Entity selectedEntity = INVALID_ENTITY;

        ImGui::Begin("Scene Hierarchy");

        if (ImGui::Button("Save Scene")) {
            gameplayScene.getScene().saveToFile("assets/scene.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Scene")) {
            gameplayScene.getScene().loadFromFile("assets/scene.json");
            gameplayScene.reinitializePhysics();
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Entity")) {
            selectedEntity = gameplayScene.createDefaultEntity();
        }

        for (Entity entity : gameplayScene.getScene().getAllEntities()) {
            std::string label = "Entity " + std::to_string(entity);
            if (ImGui::Selectable(label.c_str(), selectedEntity == entity)) {
                selectedEntity = entity;
            }
        }
        ImGui::End();

        ImGui::Begin("Inspector");
        if (selectedEntity != INVALID_ENTITY) {
            Scene& scene = gameplayScene.getScene();

            if (scene.hasComponent<TransformComponent>(selectedEntity)) {
                auto& transform = scene.getComponent<TransformComponent>(selectedEntity);
                ImGui::Text("Transform");
                ImGui::DragFloat("X", &transform.x, 1.0f);
                ImGui::DragFloat("Y", &transform.y, 1.0f);
                ImGui::DragInt("Width", &transform.width, 1);
                ImGui::DragInt("Height", &transform.height, 1);
            }

            if (scene.hasComponent<SpriteComponent>(selectedEntity)) {
                auto& sprite = scene.getComponent<SpriteComponent>(selectedEntity);
                ImGui::Text("Sprite");
                float color[4] = { sprite.r / 255.0f, sprite.g / 255.0f, sprite.b / 255.0f, sprite.a / 255.0f };
                if (ImGui::ColorEdit4("Color", color)) {
                    sprite.r = static_cast<Uint8>(color[0] * 255.0f);
                    sprite.g = static_cast<Uint8>(color[1] * 255.0f);
                    sprite.b = static_cast<Uint8>(color[2] * 255.0f);
                    sprite.a = static_cast<Uint8>(color[3] * 255.0f);
                }
            }

            if (ImGui::Button("Delete Entity")) {
                scene.destroyEntity(selectedEntity);
                selectedEntity = INVALID_ENTITY;
            }
        } else {
            ImGui::Text("No entity selected");
        }
        ImGui::End();

        SDL_SetRenderDrawColor(renderer, 30, 30, 60, 255);
        SDL_RenderClear(renderer);

        gameplayScene.render(renderer);

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    audio.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}