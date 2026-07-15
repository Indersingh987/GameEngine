#include <SDL2/SDL.h>
#include <iostream>
#include "Engine/AudioManager.h"
#include "Engine/TextureManager.h"
#include "Game/GameplayScene.h"
#include "imgui.h"
#include "imgui_internal.h" // DockBuilder* - programmatic default dock layout, first run only
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>

// Screen-space -> world-space conversion. 1:1 today (no camera/zoom yet); kept as its
// own function so a future camera/zoom transform has a single place to hook in.
static ImVec2 screenToWorld(int screenX, int screenY) {
    return ImVec2(static_cast<float>(screenX), static_cast<float>(screenY));
}

// Recursively finds every .lua file under assets/scripts/ (entity scripts at the root, Game/
// Scene scripts under game/ and scene/) for the Script Browser panel. Rescanned on demand via
// its Refresh button, not every frame - the script set only changes when a file is added.
static std::vector<std::string> scanLuaScripts() {
    std::vector<std::string> paths;
    const std::filesystem::path root = "assets/scripts";
    if (!std::filesystem::exists(root)) {
        return paths;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            paths.push_back(entry.path().generic_string());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    AudioManager audio;
    if (!audio.init()) {
        std::cerr << "Audio initialization failed!" << std::endl;
    }
    audio.loadSound("jump", "assets/jump.ogg");

    TextureManager textures;
    textures.init(renderer);

   GameplayScene gameplayScene(audio, textures);

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
        static char newEntityNameBuffer[128] = "";
        static int newEntityBodyTypeIndex = 2; // Dynamic
        static bool newEntityIsPlayer = false;
        static char newEntityScriptPathBuffer[256] = "";
        const char* bodyTypeNames[] = { "Static", "Kinematic", "Dynamic" };

        // Deferred rather than calling OpenPopup directly from inside the menu: OpenPopup and
        // BeginPopupModal hash their string id against the CURRENT id stack, and the menu bar's
        // id stack differs from the top-level context BeginPopupModal is called in below - the
        // mismatch silently prevented the popup from ever opening. Setting a flag and calling
        // OpenPopup at the same (top) scope as BeginPopupModal keeps both hashes identical.
        static bool requestCreateEntityPopup = false;

        static bool showSceneHierarchy = true;
        static bool showInspector = true;
        static bool showScriptBrowser = true;

        if (ImGui::BeginMainMenuBar()) {
            // Scene-editing operations are edit-mode-only - editing scene data or structure
            // while Play is running would fight with the live simulation state.
            ImGui::BeginDisabled(gameplayScene.isPlaying());
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Scene")) {
                    gameplayScene.getScene().saveToFile("assets/scene.json");
                }
                if (ImGui::MenuItem("Load Scene")) {
                    try {
                        gameplayScene.getScene().loadFromFile("assets/scene.json");
                        gameplayScene.reinitializePhysics();
                        gameplayScene.reinitializeTextures();
                        selectedEntity = INVALID_ENTITY;
                    } catch (const std::exception& e) {
                        std::cerr << "Load Scene failed: " << e.what() << std::endl;
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Load Scene Error", e.what(), window);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Scene")) {
                if (ImGui::MenuItem("Create Entity...")) {
                    requestCreateEntityPopup = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndDisabled();

            // Panel visibility isn't scene data - stays togglable during Play too.
            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("Scene Hierarchy", nullptr, &showSceneHierarchy);
                ImGui::MenuItem("Inspector", nullptr, &showInspector);
                ImGui::MenuItem("Script Browser", nullptr, &showScriptBrowser);
                ImGui::EndMenu();
            }

            // Play/Stop stays enabled regardless of Play state - it IS the toggle. Right-aligned,
            // sized to fit whichever label is currently showing.
            const char* playStopLabel = gameplayScene.isPlaying() ? "Stop" : "Play";
            float buttonWidth = ImGui::CalcTextSize(playStopLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonWidth - 12.0f);
            if (!gameplayScene.isPlaying()) {
                if (ImGui::Button("Play")) {
                    gameplayScene.play();
                }
            } else {
                if (ImGui::Button("Stop")) {
                    gameplayScene.stop();
                    selectedEntity = INVALID_ENTITY;
                }
            }

            ImGui::EndMainMenuBar();
        }

        if (requestCreateEntityPopup) {
            requestCreateEntityPopup = false;
            newEntityNameBuffer[0] = '\0';
            newEntityBodyTypeIndex = 2;
            newEntityIsPlayer = false;
            newEntityScriptPathBuffer[0] = '\0';
            ImGui::OpenPopup("Create Entity");
        }

        // Popups render as independent overlays, not nested in the menu bar that opens them -
        // this must still be called every frame regardless of window context.
        if (ImGui::BeginPopupModal("Create Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Name", newEntityNameBuffer, sizeof(newEntityNameBuffer));
            ImGui::Combo("Body Type", &newEntityBodyTypeIndex, bodyTypeNames, IM_ARRAYSIZE(bodyTypeNames));
            ImGui::Checkbox("Is Player-Controlled", &newEntityIsPlayer);
            ImGui::InputText("Script Path", newEntityScriptPathBuffer, sizeof(newEntityScriptPathBuffer));

            if (ImGui::Button("Create")) {
                std::string name = newEntityNameBuffer[0] != '\0' ? newEntityNameBuffer : "Entity";
                BodyType bodyType = static_cast<BodyType>(newEntityBodyTypeIndex);
                std::string scriptPath = newEntityScriptPathBuffer;
                selectedEntity = gameplayScene.createEntity(name, bodyType, newEntityIsPlayer, scriptPath);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Passthrough central node: panels dock around the edges, but the existing edit-mode
        // scene viewport (drag-to-position below) keeps showing through the undocked center,
        // rather than the dockspace host window painting an opaque background over it.
        ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

        // DockBuilderGetNode returns non-null once this dockspace has EITHER been built below OR
        // restored from a saved imgui.ini - so this default layout only ever applies on a truly
        // fresh ini (or first run), never overwriting a user's own saved arrangement.
        //
        // dockspaceId itself is never reassigned below - per imgui_internal.h's own comment on
        // DockBuilderSplitNode, "the initial node becomes a parent node", so it stays valid as
        // the whole-area root throughout, no matter how many descendants get split off it. Every
        // split writes its two new child ids into their own dedicated variables instead.
        if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

            ImGuiID bottomId = 0;
            ImGuiID topId = 0;
            ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.22f, &bottomId, &topId);

            ImGuiID rightId = 0;
            ImGui::DockBuilderSplitNode(topId, ImGuiDir_Right, 0.28f, &rightId, nullptr);

            ImGuiID rightBottomId = 0;
            ImGuiID rightTopId = 0;
            ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.5f, &rightBottomId, &rightTopId);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", rightTopId);
            ImGui::DockBuilderDockWindow("Inspector", rightBottomId);
            ImGui::DockBuilderDockWindow("Script Browser", bottomId);
            ImGui::DockBuilderFinish(dockspaceId);
        }

        ImGui::DockSpaceOverViewport(dockspaceId, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

        static Entity draggedEntity = INVALID_ENTITY;
        static float dragOffsetX = 0.0f;
        static float dragOffsetY = 0.0f;

        // Drag-to-position: only while editing (not Play mode), and only starts a new drag
        // when the mouse isn't over an ImGui panel - but an in-progress drag keeps updating
        // even if the cursor strays over a panel mid-drag.
        if (!gameplayScene.isPlaying()) {
            int mouseScreenX, mouseScreenY;
            Uint32 mouseButtons = SDL_GetMouseState(&mouseScreenX, &mouseScreenY);
            bool leftMouseDown = (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            ImVec2 worldPos = screenToWorld(mouseScreenX, mouseScreenY);
            Scene& dragScene = gameplayScene.getScene();

            if (leftMouseDown && draggedEntity == INVALID_ENTITY && !ImGui::GetIO().WantCaptureMouse) {
                const auto& entities = dragScene.getAllEntities();
                for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
                    Entity entity = *it;
                    if (!dragScene.hasComponent<TransformComponent>(entity)) continue;
                    auto& t = dragScene.getComponent<TransformComponent>(entity);
                    if (worldPos.x >= t.x && worldPos.x <= t.x + t.width &&
                        worldPos.y >= t.y && worldPos.y <= t.y + t.height) {
                        draggedEntity = entity;
                        selectedEntity = entity;
                        dragOffsetX = worldPos.x - t.x;
                        dragOffsetY = worldPos.y - t.y;
                        break;
                    }
                }
            }

            if (leftMouseDown && draggedEntity != INVALID_ENTITY &&
                dragScene.hasComponent<TransformComponent>(draggedEntity)) {
                auto& t = dragScene.getComponent<TransformComponent>(draggedEntity);
                t.x = worldPos.x - dragOffsetX;
                t.y = worldPos.y - dragOffsetY;
            }

            if (!leftMouseDown) {
                draggedEntity = INVALID_ENTITY;
            }
        } else {
            draggedEntity = INVALID_ENTITY;
        }

        if (showSceneHierarchy) {
            ImGui::Begin("Scene Hierarchy", &showSceneHierarchy);

            for (Entity entity : gameplayScene.getScene().getAllEntities()) {
                Scene& hierarchyScene = gameplayScene.getScene();
                std::string label;
                if (hierarchyScene.hasComponent<TagComponent>(entity) &&
                    !hierarchyScene.getComponent<TagComponent>(entity).displayName.empty()) {
                    label = hierarchyScene.getComponent<TagComponent>(entity).displayName;
                } else {
                    label = "Entity " + std::to_string(entity);
                }
                if (ImGui::Selectable(label.c_str(), selectedEntity == entity)) {
                    selectedEntity = entity;
                }
            }
            ImGui::End();
        }

        if (showInspector) {
            ImGui::Begin("Inspector", &showInspector);
            // Editing entity data while Play is running would fight with the live simulation -
            // same edit-mode-only rule as Save/Load/Create Entity above.
            ImGui::BeginDisabled(gameplayScene.isPlaying());
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

                    static char texturePathBuffer[256] = "";
                    ImGui::InputText("Texture Path", texturePathBuffer, sizeof(texturePathBuffer));
                    ImGui::SameLine();
                    if (ImGui::Button("Load Texture")) {
                        std::string path(texturePathBuffer);
                        sprite.texture = textures.loadTexture(path, path);
                        sprite.texturePath = path;
                    }
                    if (!sprite.texturePath.empty()) {
                        ImGui::Text("Current: %s", sprite.texturePath.c_str());
                        if (ImGui::Button("Clear Texture")) {
                            sprite.texturePath.clear();
                            sprite.texture = nullptr;
                        }
                    }
                }

                if (ImGui::Button("Delete Entity")) {
                    scene.destroyEntity(selectedEntity);
                    selectedEntity = INVALID_ENTITY;
                }
            } else {
                ImGui::Text("No entity selected");
            }
            ImGui::EndDisabled();
            ImGui::End();
        }

        static std::vector<std::string> luaScripts = scanLuaScripts();
        if (showScriptBrowser) {
            ImGui::Begin("Script Browser", &showScriptBrowser);
            if (ImGui::Button("Refresh")) {
                luaScripts = scanLuaScripts();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Click a path to copy it");
            ImGui::Separator();
            for (const std::string& path : luaScripts) {
                if (ImGui::Selectable(path.c_str())) {
                    ImGui::SetClipboardText(path.c_str());
                }
            }
            ImGui::End();
        }

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