cmake_minimum_required(VERSION 3.20)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(box2d CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(Lua REQUIRED)

# --- Engine library ---
add_library(Engine STATIC
    Engine/src/Engine/AudioManager.cpp
    Engine/src/Engine/TextureManager.cpp
    Engine/src/Engine/ScriptManager.cpp
    Engine/src/Engine/TierScript.cpp
    Engine/src/Engine/ECS/Scene.cpp
    Engine/src/Engine/ECS/Systems.cpp
    external/imgui_backends/imgui_impl_sdl2.cpp
    external/imgui_backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(Engine PUBLIC Engine/include)
target_link_libraries(Engine PUBLIC SDL2::SDL2 SDL2::SDL2main SDL2_mixer::SDL2_mixer $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static> box2d::box2d imgui::imgui nlohmann_json::nlohmann_json sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(Engine PUBLIC Engine/include external/imgui_backends ${LUA_INCLUDE_DIR})

# --- Game executable ---
add_executable(Game
    Game/src/Game/main.cpp
    Game/src/Game/GameplayScene.cpp
    external/ImGuiColorTextEdit/TextEditor.cpp
)
target_include_directories(Game PRIVATE Game/include external/ImGuiColorTextEdit)
target_link_libraries(Game PRIVATE Engine)