#include "AudioManager.h"
#include <iostream>

AudioManager::AudioManager() {
}

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::init() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
        return false;
    }
    return true;
}

void AudioManager::shutdown() {
    for (auto& pair : sounds) {
        Mix_FreeChunk(pair.second);
    }
    sounds.clear();

    for (auto& pair : musicTracks) {
        Mix_FreeMusic(pair.second);
    }
    musicTracks.clear();

    Mix_CloseAudio();
}

bool AudioManager::loadSound(const std::string& name, const std::string& filepath) {
    Mix_Chunk* chunk = Mix_LoadWAV(filepath.c_str());
    if (chunk == nullptr) {
        std::cerr << "Failed to load sound '" << name << "': " << Mix_GetError() << std::endl;
        return false;
    }
    sounds[name] = chunk;
    return true;
}

void AudioManager::playSound(const std::string& name) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        int channel = Mix_PlayChannel(-1, it->second, 0);
        if (channel == -1) {
            std::cerr << "Mix_PlayChannel failed: " << Mix_GetError() << std::endl;
        } else {
            std::cerr << "Playing on channel: " << channel << std::endl;
        }
    } else {
        std::cerr << "Sound not found: " << name << std::endl;
    }
}

bool AudioManager::loadMusic(const std::string& name, const std::string& filepath) {
    Mix_Music* music = Mix_LoadMUS(filepath.c_str());
    if (music == nullptr) {
        std::cerr << "Failed to load music '" << name << "': " << Mix_GetError() << std::endl;
        return false;
    }
    musicTracks[name] = music;
    return true;
}

void AudioManager::playMusic(const std::string& name, bool loop) {
    auto it = musicTracks.find(name);
    if (it != musicTracks.end()) {
        Mix_PlayMusic(it->second, loop ? -1 : 1);
    } else {
        std::cerr << "Music not found: " << name << std::endl;
    }
}

void AudioManager::stopMusic() {
    Mix_HaltMusic();
}