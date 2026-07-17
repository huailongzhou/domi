#include "domi/app.h"
#include "domi/input.h"
#include "domi/script.h"
#include "domi/render.h"
#include "domi/audio.h"
#include "domi/scene_manager.h"
#include "domi/thread_pool.h"
#include "domi/backend/sdl_backend.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <thread>

namespace domi {

App& App::instance() {
    static App app;
    return app;
}

App::App()
    : backend_(NULL), input_(NULL), script_(NULL), render_(NULL), audio_(NULL),
      sceneManager_(NULL), threadPool_(NULL), running_(false), initialized_(false),
      deltaTime_(0), totalTime_(0), fixedTime_(1.0 / 60.0), fixedAccumulator_(0),
      frameCount_(0), currentFps_(0.0f) {}

App::~App() {
    if (initialized_) shutdown();
}

bool App::init(const AppConfig& config) {
    if (initialized_) return true;

    if (!SDLBackend::initializePlatform()) {
        return false;
    }

    backend_ = new SDLBackend();
    if (!backend_->create(config.title, config.width, config.height)) {
        delete backend_;
        backend_ = NULL;
        SDLBackend::shutdownPlatform();
        return false;
    }

    if (!window_.init(backend_)) {
        backend_->destroy();
        delete backend_;
        backend_ = NULL;
        SDLBackend::shutdownPlatform();
        return false;
    }

    input_ = new InputSystem(backend_);
    script_ = new ScriptSystem();
    render_ = new RenderSystem(backend_, backend_);
    audio_ = new AudioSystem(backend_);
    sceneManager_ = new SceneManager();

    size_t threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 1;
    threadPool_ = new ThreadPool(threadCount);

    if (!script_->init()) {
        fprintf(stderr, "ScriptSystem init failed\n");
        shutdown();
        return false;
    }

    if (!render_->init()) {
        fprintf(stderr, "RenderSystem init failed\n");
        shutdown();
        return false;
    }

    if (!input_->init()) {
        fprintf(stderr, "InputSystem init failed\n");
    }

    if (!audio_->init()) {
        fprintf(stderr, "AudioSystem init failed\n");
    }

    initialized_ = true;
    running_ = true;
    totalTime_ = 0;
    frameCount_ = 0;

    return true;
}

void App::run() {
    uint64_t lastTime = SDL_GetTicksNS();
    const uint64_t nsPerSec = 1000000000ULL;

    double fpsAccumulator = 0.0;
    int fpsFrames = 0;
    double fpsTimer = 0.0;

    while (running_) {
        uint64_t now = SDL_GetTicksNS();
        double dt = (now - lastTime) / (double)nsPerSec;
        lastTime = now;

        if (dt > 0.25) dt = 0.25;
        deltaTime_ = dt;
        totalTime_ += dt;
        frameCount_++;

        if (dt > 0.0) {
            fpsAccumulator += 1.0 / dt;
            fpsFrames++;
            fpsTimer += dt;
            if (fpsTimer >= 0.25) {
                currentFps_ = static_cast<float>(fpsAccumulator / fpsFrames);
                fpsAccumulator = 0.0;
                fpsFrames = 0;
                fpsTimer = 0.0;
            }
        }

        input_->update();
        processEvents();

        fixedAccumulator_ += dt;
        while (fixedAccumulator_ >= fixedTime_) {
            fixedUpdate();
            fixedAccumulator_ -= fixedTime_;
        }

        update(dt);
        render();
    }
}

void App::processEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            running_ = false;
        }
        input_->handleEvent(e);
        if (eventHook_) eventHook_(e);
    }
}

void App::update(double dt) {
    if (sceneManager_) sceneManager_->update(&world_, script_, dt);
    if (script_) script_->update(dt);
    if (audio_) audio_->update(dt);
}

void App::fixedUpdate() {
    if (sceneManager_) sceneManager_->fixedUpdate(script_);
    if (script_) script_->fixedUpdate();
}

void App::render() {
    if (render_) render_->render(&world_, sceneManager_, currentFps_);
}

void App::shutdown() {
    running_ = false;

    if (sceneManager_) {
        sceneManager_->clear(&world_, script_);
        delete sceneManager_;
        sceneManager_ = NULL;
    }
    delete audio_; audio_ = NULL;
    delete render_; render_ = NULL;
    delete script_; script_ = NULL;
    delete threadPool_; threadPool_ = NULL;
    delete input_; input_ = NULL;

    window_.destroy();
    if (backend_) {
        backend_->destroy();
        delete backend_;
        backend_ = NULL;
    }
    SDLBackend::shutdownPlatform();
    initialized_ = false;
}

} // namespace domi
