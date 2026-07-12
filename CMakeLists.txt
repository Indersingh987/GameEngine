cmake_minimum_required(VERSION 3.20)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)

add_executable(GameEngine src/main.cpp src/Entity.cpp src/AudioManager.cpp src/GameplayScene.cpp)
target_include_directories(GameEngine PRIVATE include)

target_link_libraries(GameEngine PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2_mixer::SDL2_mixer)