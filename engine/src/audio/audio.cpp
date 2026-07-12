#include "domi/audio.h"
#include <SDL3/SDL.h>
#include <cstdio>

namespace domi {

AudioSystem::AudioSystem() : initialized_(false) {}

AudioSystem::~AudioSystem() {
    shutdown();
}

bool AudioSystem::init() {
    if (initialized_) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        return false;
    }
    initialized_ = true;
    return true;
}

void AudioSystem::shutdown() {
    if (initialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        initialized_ = false;
    }
}

void AudioSystem::update(double dt) {
    (void)dt;
}

void AudioSystem::play(const char* path, bool loop) {
    (void)path; (void)loop;
    // TODO: implement audio playback
}

void AudioSystem::stop(const char* path) {
    (void)path;
}

void AudioSystem::stopAll() {}

} // namespace domi
