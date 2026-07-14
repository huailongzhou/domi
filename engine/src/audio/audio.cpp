#include "domi/audio.h"
#include "domi/backend/audio_backend.h"
#include <cstdio>

namespace domi {

AudioSystem::AudioSystem(IAudioBackend* backend)
    : backend_(backend) {}

AudioSystem::~AudioSystem() {
    shutdown();
}

bool AudioSystem::init() {
    if (!backend_) return false;
    return backend_->init();
}

void AudioSystem::shutdown() {
    if (backend_) backend_->shutdown();
}

void AudioSystem::update(double dt) {
    (void)dt;
}

void AudioSystem::play(const char* path, bool loop) {
    if (backend_) backend_->play(path, loop);
}

void AudioSystem::stop(const char* path) {
    if (backend_) backend_->stop(path);
}

void AudioSystem::stopAll() {
    if (backend_) backend_->stopAll();
}

} // namespace domi
