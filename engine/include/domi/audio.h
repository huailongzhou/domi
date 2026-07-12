#ifndef DOMI_AUDIO_H
#define DOMI_AUDIO_H

namespace domi {

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool init();
    void shutdown();
    void update(double dt);

    void play(const char* path, bool loop);
    void stop(const char* path);
    void stopAll();

private:
    bool initialized_;
};

} // namespace domi

#endif
