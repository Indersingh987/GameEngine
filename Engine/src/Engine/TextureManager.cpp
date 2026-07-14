#include "Engine/TextureManager.h"
#include <SDL2/SDL_image.h>
#include <iostream>

TextureManager::TextureManager() {
}

TextureManager::~TextureManager() {
    shutdown();
}

bool TextureManager::init(SDL_Renderer* rendererIn) {
    renderer = rendererIn;
    return true;
}

void TextureManager::shutdown() {
    for (auto& pair : textures) {
        SDL_DestroyTexture(pair.second);
    }
    textures.clear();
}

SDL_Texture* TextureManager::loadTexture(const std::string& name, const std::string& filepath) {
    auto it = textures.find(name);
    if (it != textures.end()) {
        return it->second;
    }

    SDL_Texture* texture = IMG_LoadTexture(renderer, filepath.c_str());
    if (texture == nullptr) {
        std::cerr << "Failed to load texture '" << name << "' from '" << filepath << "': " << IMG_GetError() << std::endl;
        return nullptr;
    }
    textures[name] = texture;
    return texture;
}
