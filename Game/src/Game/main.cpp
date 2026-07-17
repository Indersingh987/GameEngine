#include <SDL2/SDL.h>
#include <box2d/box2d.h>
#include <iostream>
#include "Engine/AudioManager.h"
#include "Engine/TextureManager.h"
#include "Game/GameplayScene.h"
#include "imgui.h"
#include "imgui_internal.h" // DockBuilder* - programmatic default dock layout, first run only
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "TextEditor.h"
#include <string>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <vector>
#include <algorithm>
#include <cstring>

// Editor-viewport-only preview draw: renders the given entity at a fixed centered position
// (mirrors Systems::render's texture-vs-flat-color logic exactly, but overrides x/y - this is
// display-only, never written back to the entity's real TransformComponent). Distinct from
// Systems::render/GameplayScene::render, which the Playground window still uses unchanged to
// show the full live scene at real positions.
static void renderEntityPreview(SDL_Renderer* renderer, Scene& scene, Entity entity) {
    if (!scene.hasComponent<TransformComponent>(entity) || !scene.hasComponent<SpriteComponent>(entity)) {
        return;
    }
    auto& transform = scene.getComponent<TransformComponent>(entity);
    auto& sprite = scene.getComponent<SpriteComponent>(entity);

    constexpr int previewCenterX = 400;
    constexpr int previewCenterY = 300;

    SDL_Rect rect;
    rect.w = transform.width;
    rect.h = transform.height;
    rect.x = previewCenterX - rect.w / 2;
    rect.y = previewCenterY - rect.h / 2;

    if (sprite.texture != nullptr) {
        SDL_RenderCopy(renderer, sprite.texture, nullptr, &rect);
    } else {
        SDL_SetRenderDrawColor(renderer, sprite.r, sprite.g, sprite.b, sprite.a);
        SDL_RenderFillRect(renderer, &rect);
    }
}

// Recursively finds every .lua file under assets/scripts/ (entity scripts under Entities/, Game/
// Scene scripts under Game/ and Scenes/) for the Script Browser panel. Rescanned on demand via
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

    // Playground: a genuinely separate OS window + renderer for Play-mode testing, created on
    // Play and torn down on Stop - not a docked panel, not render-to-texture, not ImGui's
    // multi-viewport mode (all explicitly rejected per the editor layout spec). The main
    // window's own passthrough viewport keeps showing the edit-mode scene throughout; Play mode
    // does not change what the main window renders, it only adds this second live view.
    SDL_Window* playgroundWindow = nullptr;
    SDL_Renderer* playgroundRenderer = nullptr;

    // Declared before the event loop (rather than alongside the other UI statics further down)
    // since the playground-window-close handler below needs to reset it too. No longer `static`
    // since it's declared once outside the loop now, not re-entered inside it.
    Entity selectedEntity = INVALID_ENTITY;

    while (running) {
        Uint64 currentTime = SDL_GetTicks64();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (playgroundWindow != nullptr && event.window.windowID == SDL_GetWindowID(playgroundWindow)) {
                    // Closing just the playground window stops Play (and tears the window down
                    // with it) rather than quitting the whole editor.
                    gameplayScene.stop();
                    selectedEntity = INVALID_ENTITY;
                    SDL_DestroyRenderer(playgroundRenderer);
                    SDL_DestroyWindow(playgroundWindow);
                    playgroundRenderer = nullptr;
                    playgroundWindow = nullptr;
                } else if (event.window.windowID == SDL_GetWindowID(window)) {
                    running = false;
                }
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
        static bool requestGameScriptPopup = false;
        static bool requestSceneScriptPopup = false;
        static char gameScriptPathBuffer[256] = "";
        static char sceneScriptPathBuffer[256] = "";
        static std::string gameScriptError;
        static std::string sceneScriptError;

        static bool showSceneHierarchy = true;
        static bool showInspector = true;
        static bool showScriptBrowser = true;

        if (ImGui::BeginMainMenuBar()) {
            // Scene-editing operations are edit-mode-only - editing scene data or structure
            // while Play is running would fight with the live simulation state.
            ImGui::BeginDisabled(gameplayScene.isPlaying());
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Scene")) {
                    gameplayScene.getScene().saveToFile(gameplayScene.getScenePath());
                }
                if (ImGui::MenuItem("Load Scene")) {
                    try {
                        gameplayScene.getScene().loadFromFile(gameplayScene.getScenePath());
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
                if (ImGui::MenuItem("Scene Script...")) {
                    requestSceneScriptPopup = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Scripts")) {
                if (ImGui::MenuItem("Game Script...")) {
                    requestGameScriptPopup = true;
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
                    playgroundWindow = SDL_CreateWindow(
                        "Playground", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                        800, 600, SDL_WINDOW_SHOWN
                    );
                    if (playgroundWindow != nullptr) {
                        playgroundRenderer = SDL_CreateRenderer(playgroundWindow, -1, SDL_RENDERER_ACCELERATED);
                    }
                    if (playgroundWindow == nullptr || playgroundRenderer == nullptr) {
                        std::cerr << "Failed to create playground window: " << SDL_GetError() << std::endl;
                        if (playgroundWindow != nullptr) {
                            SDL_DestroyWindow(playgroundWindow);
                            playgroundWindow = nullptr;
                        }
                        gameplayScene.stop();
                    }
                }
            } else {
                if (ImGui::Button("Stop")) {
                    gameplayScene.stop();
                    selectedEntity = INVALID_ENTITY;
                    if (playgroundRenderer != nullptr) {
                        SDL_DestroyRenderer(playgroundRenderer);
                        playgroundRenderer = nullptr;
                    }
                    if (playgroundWindow != nullptr) {
                        SDL_DestroyWindow(playgroundWindow);
                        playgroundWindow = nullptr;
                    }
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

        if (requestGameScriptPopup) {
            requestGameScriptPopup = false;
            gameScriptError.clear();
            std::strncpy(gameScriptPathBuffer, gameplayScene.getGameScriptPath().c_str(), sizeof(gameScriptPathBuffer) - 1);
            gameScriptPathBuffer[sizeof(gameScriptPathBuffer) - 1] = '\0';
            ImGui::OpenPopup("Game Script");
        }

        if (ImGui::BeginPopupModal("Game Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextDisabled("The one Game script - persists for the whole app, survives scene changes.");
            ImGui::InputText("Path", gameScriptPathBuffer, sizeof(gameScriptPathBuffer));
            if (ImGui::Button("Apply")) {
                if (gameplayScene.setGameScriptPath(gameScriptPathBuffer)) {
                    ImGui::CloseCurrentPopup();
                } else {
                    gameScriptError = "Failed to load that script - check the path and Lua syntax.";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            if (!gameScriptError.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", gameScriptError.c_str());
            }
            ImGui::EndPopup();
        }

        if (requestSceneScriptPopup) {
            requestSceneScriptPopup = false;
            sceneScriptError.clear();
            std::strncpy(sceneScriptPathBuffer, gameplayScene.getSceneScriptPath().c_str(), sizeof(sceneScriptPathBuffer) - 1);
            sceneScriptPathBuffer[sizeof(sceneScriptPathBuffer) - 1] = '\0';
            ImGui::OpenPopup("Scene Script");
        }

        if (ImGui::BeginPopupModal("Scene Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextDisabled("This scene's script - mediates its entities, talks up to the Game script.");
            ImGui::InputText("Path", sceneScriptPathBuffer, sizeof(sceneScriptPathBuffer));
            if (ImGui::Button("Apply")) {
                if (gameplayScene.setSceneScriptPath(sceneScriptPathBuffer)) {
                    ImGui::CloseCurrentPopup();
                } else {
                    sceneScriptError = "Failed to load that script - check the path and Lua syntax.";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            if (!sceneScriptError.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", sceneScriptError.c_str());
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
                // "##<id>" scopes the widget's ID to the entity's unique id without displaying
                // it - Selectable() otherwise uses the visible label itself as the ID, so two
                // entities with the same display name (e.g. both "Test") collide.
                std::string widgetId = label + "##" + std::to_string(entity);
                if (ImGui::Selectable(widgetId.c_str(), selectedEntity == entity)) {
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

                if (scene.hasComponent<TagComponent>(selectedEntity)) {
                    auto& tag = scene.getComponent<TagComponent>(selectedEntity);
                    ImGui::Text("Tag");

                    char nameBuffer[256];
                    std::strncpy(nameBuffer, tag.displayName.c_str(), sizeof(nameBuffer) - 1);
                    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
                    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
                        tag.displayName = nameBuffer;
                    }

                    // Same unclaim-previous-player logic as Create Entity's checkbox
                    // (GameplayScene::createEntity), plus a self-guard since - unlike a freshly
                    // created entity - this one might already BE the current player.
                    bool isPlayer = (tag.role == "player");
                    if (ImGui::Checkbox("Is Player-Controlled", &isPlayer)) {
                        if (isPlayer) {
                            Entity previousPlayer = scene.findEntityByRole("player");
                            if (previousPlayer != INVALID_ENTITY && previousPlayer != selectedEntity) {
                                scene.getComponent<TagComponent>(previousPlayer).role = "";
                            }
                            tag.role = "player";
                        } else {
                            tag.role = "";
                        }
                    }
                }

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

                if (scene.hasComponent<PhysicsComponent>(selectedEntity)) {
                    auto& phys = scene.getComponent<PhysicsComponent>(selectedEntity);
                    ImGui::Text("Physics");
                    int bodyTypeIndex = static_cast<int>(phys.bodyType);
                    if (ImGui::Combo("Body Type", &bodyTypeIndex, bodyTypeNames, IM_ARRAYSIZE(bodyTypeNames))) {
                        // createPhysicsBody doesn't destroy an existing body first (Scene.cpp) -
                        // do it ourselves, same B2_IS_NULL/b2DestroyBody pattern Scene::destroyEntity
                        // and Scene::clear already use, then rebuild with the new body type.
                        if (!B2_IS_NULL(phys.bodyId)) {
                            b2DestroyBody(phys.bodyId);
                            phys.bodyId = b2_nullBodyId;
                        }
                        phys.bodyType = static_cast<BodyType>(bodyTypeIndex);
                        scene.createPhysicsBody(selectedEntity);
                    }
                }

                if (scene.hasComponent<ScriptComponent>(selectedEntity)) {
                    auto& scriptComp = scene.getComponent<ScriptComponent>(selectedEntity);
                    ImGui::Text("Script");
                    char scriptPathBuffer[256];
                    std::strncpy(scriptPathBuffer, scriptComp.scriptPath.c_str(), sizeof(scriptPathBuffer) - 1);
                    scriptPathBuffer[sizeof(scriptPathBuffer) - 1] = '\0';
                    if (ImGui::InputText("Script Path", scriptPathBuffer, sizeof(scriptPathBuffer))) {
                        scriptComp.scriptPath = scriptPathBuffer;
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

        // Script Editor state (step 3 of the build order in CLAUDE.md): which script, if any, is
        // currently open, and the widget showing it. Declared here (rather than inside either
        // panel's own block) so both the Script Browser block below and the Script Editor block
        // can see the same statics - a static declared inside a nested block is only visible in
        // that block.
        static std::string openScriptPath;
        static std::string saveStatus;
        static bool isDirty = false;
        static std::string pendingOpenPath;      // step 5: path waiting on a discard confirmation
        static bool requestDiscardConfirmPopup = false;
        static TextEditor scriptEditor = [] {
            TextEditor editor;
            editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
            return editor;
        }();

        // Loads path's file content into the editor, discarding whatever was open before -
        // callers are responsible for having already confirmed that's OK (see the discard-confirm
        // popup below, decision 6 of the build order).
        auto openScriptFile = [&](const std::string& path) {
            std::ifstream in(path);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            scriptEditor.SetText(content);
            scriptEditor.SetErrorMarkers({});
            openScriptPath = path;
            saveStatus.clear();
            isDirty = false;
        };

        if (showScriptBrowser) {
            ImGui::Begin("Script Browser", &showScriptBrowser);
            if (ImGui::Button("Refresh")) {
                luaScripts = scanLuaScripts();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Click a path to open it in the Script Editor");
            ImGui::Separator();
            for (const std::string& path : luaScripts) {
                if (ImGui::Selectable(path.c_str(), path == openScriptPath)) {
                    if (path != openScriptPath) {
                        if (isDirty) {
                            pendingOpenPath = path;
                            requestDiscardConfirmPopup = true;
                        } else {
                            openScriptFile(path);
                        }
                    }
                }
            }
            ImGui::End();
        }

        // Must be called every frame regardless of Script Browser's own visibility, same reason
        // as the Create Entity/Game Script/Scene Script popups above - ImGui needs OpenPopup and
        // BeginPopupModal to both run every frame for the popup state machine to work.
        if (requestDiscardConfirmPopup) {
            requestDiscardConfirmPopup = false;
            ImGui::OpenPopup("Discard Unsaved Changes?");
        }
        if (ImGui::BeginPopupModal("Discard Unsaved Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Unsaved edits in:\n%s\n\nDiscard them and open:\n%s?",
                        openScriptPath.c_str(), pendingOpenPath.c_str());
            if (ImGui::Button("Discard and Open")) {
                openScriptFile(pendingOpenPath);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Begin("Script Editor");
        if (openScriptPath.empty()) {
            ImGui::TextDisabled("No script open - click a path in Script Browser to open it.");
        } else {
            ImGui::TextUnformatted((openScriptPath + (isDirty ? " *" : "")).c_str());
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                std::string syntaxError;
                int syntaxErrorLine = 1;
                std::string currentText = scriptEditor.GetText();
                if (!gameplayScene.checkScriptSyntax(currentText, openScriptPath, syntaxError, syntaxErrorLine)) {
                    // Caught before ever touching disk - the file on disk keeps whatever its
                    // last-saved (valid) content was, and no reload is attempted.
                    scriptEditor.SetErrorMarkers({{syntaxErrorLine, syntaxError}});
                    saveStatus = "Syntax error, NOT saved - see line " + std::to_string(syntaxErrorLine) + " below.";
                } else {
                    scriptEditor.SetErrorMarkers({});
                    std::ofstream out(openScriptPath, std::ios::trunc);
                    if (!out) {
                        saveStatus = "Failed to write file: " + openScriptPath;
                    } else {
                        out << currentText;
                        out.close();
                        isDirty = false; // content now matches what's on disk, regardless of reload outcome below

                        // Which reload path decision 2 requires depends on whether this path is
                        // the ACTIVE Game/Scene script right now - those need the load-then-rebind
                        // sequence (callScript/switchScene bindings), not the entity-script
                        // in-place cache reload, which would silently drop those bindings. A
                        // reload failure here should be rare now that Save only ever writes
                        // syntax-checked content, but stays possible (e.g. a runtime error
                        // resolving onUpdate) so this path is kept, not removed.
                        bool reloadOk;
                        const char* kind;
                        if (openScriptPath == gameplayScene.getGameScriptPath()) {
                            reloadOk = gameplayScene.setGameScriptPath(openScriptPath);
                            kind = "Game";
                        } else if (openScriptPath == gameplayScene.getSceneScriptPath()) {
                            reloadOk = gameplayScene.setSceneScriptPath(openScriptPath);
                            kind = "Scene";
                        } else {
                            reloadOk = gameplayScene.reloadEntityScript(openScriptPath);
                            kind = "Entity";
                        }
                        saveStatus = reloadOk
                            ? (std::string("Saved and reloaded (") + kind + " script).")
                            : (std::string("Saved, but reload FAILED (") + kind + " script) - check console for the parse error.");
                    }
                }
            }
            if (!saveStatus.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", saveStatus.c_str());
            }
            ImGui::Separator();
            scriptEditor.Render("ScriptEditor", ImVec2(0, 0), true);
            if (scriptEditor.IsTextChanged()) {
                isDirty = true;
            }
        }
        ImGui::End();

        SDL_SetRenderDrawColor(renderer, 30, 30, 60, 255);
        SDL_RenderClear(renderer);

        if (selectedEntity != INVALID_ENTITY) {
            renderEntityPreview(renderer, gameplayScene.getScene(), selectedEntity);
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);

        // Sprites with a loaded texture won't render correctly here yet - SDL_Texture is tied
        // to the renderer that created it (TextureManager only ever creates against the main
        // `renderer`), and this is a different renderer/device. Not an issue for the current
        // flat-color-only test scene; giving TextureManager a way to load per-renderer is
        // deferred until a scene actually needs a textured sprite visible in Play mode.
        if (playgroundRenderer != nullptr) {
            SDL_SetRenderDrawColor(playgroundRenderer, 30, 30, 60, 255);
            SDL_RenderClear(playgroundRenderer);
            gameplayScene.render(playgroundRenderer);
            SDL_RenderPresent(playgroundRenderer);
        }
    }

    if (playgroundRenderer != nullptr) {
        SDL_DestroyRenderer(playgroundRenderer);
    }
    if (playgroundWindow != nullptr) {
        SDL_DestroyWindow(playgroundWindow);
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