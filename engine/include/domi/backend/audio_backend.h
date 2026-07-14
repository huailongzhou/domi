#ifndef DOMI_BACKEND_AUDIO_BACKEND_H
#define DOMI_BACKEND_AUDIO_BACKEND_H

namespace domi {

// Abstract audio backend.
//
// Handles audio subsystem initialization and playback control.
class IAudioBackend {
public:
    virtual ~IAudioBackend() {}

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual void play(const char* path, bool loop) = 0;
    virtual void stop(const char* path) = 0;
    virtual void stopAll() = 0;
};

} // namespace domi

#endif // DOMI_BACKEND_AUDIO_BACKEND_H
