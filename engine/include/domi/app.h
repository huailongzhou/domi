#ifndef DOMI_APP_H
#define DOMI_APP_H

#include "domi/window.h"
#include "domi/ecs.h"
#include <cstdint>

namespace domi {

class SDLBackend;
class ThreadPool;
class ScriptSystem;
class RenderSystem;
class InputSystem;
class AudioSystem;
class SceneManager;

struct AppConfig {
    const char* title;
    int width;
    int height;
    bool vsync;
    int targetFPS;
    AppConfig() : title("Domi Engine"), width(1280), height(720), vsync(true), targetFPS(60) {}
};

class App {
public:
    static App& instance();

    bool init(const AppConfig& config);
    void run();
    void shutdown();

    Window* getWindow() { return &window_; }
    World* getWorld() { return &world_; }
    InputSystem* getInput() { return input_; }
    ScriptSystem* getScript() { return script_; }
    RenderSystem* getRender() { return render_; }
    AudioSystem* getAudio() { return audio_; }
    SceneManager* getSceneManager() { return sceneManager_; }
    ThreadPool* getThreadPool() { return threadPool_; }

    double getDeltaTime() const { return deltaTime_; }
    double getTime() const { return totalTime_; }
    uint64_t getFrameCount() const { return frameCount_; }

    bool isRunning() const { return running_; }
    void quit() { running_ = false; }

private:
    App();
    ~App();
    App(const App&);
    App& operator=(const App&);

    void processEvents();
    void update(double dt);
    void fixedUpdate();
    void render();

    SDLBackend* backend_;
    Window window_;
    World world_;
    InputSystem* input_;
    ScriptSystem* script_;
    RenderSystem* render_;
    AudioSystem* audio_;
    SceneManager* sceneManager_;
    ThreadPool* threadPool_;

    bool running_;
    bool initialized_;
    double deltaTime_;
    double totalTime_;
    double fixedTime_;
    double fixedAccumulator_;
    uint64_t frameCount_;
};

} // namespace domi

#endif
