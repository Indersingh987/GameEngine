#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>

class TextureManager {
public:
    TextureManager();
    ~TextureManager();

    bool init(SDL_Renderer* renderer);
    void shutdown();

    SDL_Texture* loadTexture(const std::string& name, const std::string& filepath);

private:
    SDL_Renderer* renderer = nullptr;
    std::unordered_map<std::string, SDL_Texture*> textures;
};
