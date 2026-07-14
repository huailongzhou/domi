#ifndef DOMI_AUDIO_H
#define DOMI_AUDIO_H

namespace domi {

class IAudioBackend;

class AudioSystem {
public:
    explicit AudioSystem(IAudioBackend* backend);
    ~AudioSystem();

    bool init();
    void shutdown();
    void update(double dt);

    void play(const char* path, bool loop);
    void stop(const char* path);
    void stopAll();

private:
    IAudioBackend* backend_;
};

} // namespace domi

#endif
