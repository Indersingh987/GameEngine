#pragma once

#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool init();
    void shutdown();

    bool loadSound(const std::string& name, const std::string& filepath);
    void playSound(const std::string& name);

    bool loadMusic(const std::string& name, const std::string& filepath);
    void playMusic(const std::string& name, bool loop = true);
    void stopMusic();

private:
    std::unordered_map<std::string, Mix_Chunk*> sounds;
    std::unordered_map<std::string, Mix_Music*> musicTracks;
};