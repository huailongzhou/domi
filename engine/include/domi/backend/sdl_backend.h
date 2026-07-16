#ifndef DOMI_BACKEND_SDL_BACKEND_H
#define DOMI_BACKEND_SDL_BACKEND_H

#include "domi/backend/render_backend.h"
#include "domi/backend/window_backend.h"
#include "domi/backend/input_backend.h"
#include "domi/backend/audio_backend.h"
#include <string>
#include <cstring>
#include <vector>
#include <memory>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_GPUDevice;
struct SDL_Texture;

namespace domi {

struct MaterialCache;

// SDL3 implementation of the window, render, input and audio backends.
//
// This is the single place where SDL3 window/renderer/texture/input/audio code
// lives. The rest of the engine talks to this class through the backend
// interfaces.
class SDLBackend : public IWindowBackend, public IRenderBackend,
                   public IInputBackend, public IAudioBackend {
public:
    SDLBackend();
    ~SDLBackend() override;

    // Global SDL3 platform initialization. Call once before create().
    static bool initializePlatform();
    static void shutdownPlatform();

    // IWindowBackend
    bool create(const std::string& title, int width, int height) override;
    void destroy() override;
    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }
    bool claimGPU() override;
    void releaseGPU() override;
    bool isGPUClaimed() const override { return gpuClaimed_; }
    void* getNativeGPUDevice() override { return gpuDevice_; }
    void* getNativeWindow() override { return window_; }

    // IRenderBackend
    void setTarget(RenderTexture* target) override;
    void clear(const Color& c) override;
    void setClipRect(float x, float y, float w, float h) override;
    void resetClipRect() override;
    void present() override;

    void setFillColor(const Color& c) override;
    void setStrokeColor(const Color& c) override;
    void setLineWidth(float w) override;

    void fillRect(float x, float y, float w, float h) override;
    void strokeRect(float x, float y, float w, float h) override;
    void drawLine(float x1, float y1, float x2, float y2) override;
    void fillCircle(float x, float y, float radius, int segments) override;

    void fillPath(const std::vector<Vec2>& points, bool closed,
                  const Color& color) override;
    void strokePath(const std::vector<Vec2>& points, bool closed,
                    const Color& color, float lineWidth) override;

    void drawTexture(float x, float y, RenderTexture* texture,
                     BlendMode mode, float angle = 0.0f,
                     float centerX = 0.0f, float centerY = 0.0f,
                     float scaleX = 1.0f, float scaleY = 1.0f) override;

    void* uploadMaterial(const Material& material) override;
    void drawMaterial(float x, float y, void* handle,
                      float angle = 0.0f,
                      float centerX = 0.0f, float centerY = 0.0f,
                      float scaleX = 1.0f, float scaleY = 1.0f) override;
    void destroyMaterial(void* handle) override;

    void* createRenderTarget(int width, int height,
                             RenderTextureFormat format) override;
    void destroyRenderTarget(void* handle) override;

    bool lock3DTarget(void** pixels, int* pitch) override;
    void unlock3DTarget() override;
    void present3DTarget() override;

    // IInputBackend
    bool init() override;
    void shutdown() override;
    void update() override;
    void handleEvent(const void* nativeEvent) override;
    bool isKeyDown(int key) const override;
    bool isKeyPressed(int key) const override;
    bool isKeyReleased(int key) const override;
    bool isMouseButtonDown(int button) const override;
    bool isMouseButtonPressed(int button) const override;
    float getMouseX() const override { return mouseX_; }
    float getMouseY() const override { return mouseY_; }
    float getMouseDeltaX() const override { return mouseDeltaX_; }
    float getMouseDeltaY() const override { return mouseDeltaY_; }
    float getScrollX() const override { return scrollX_; }
    float getScrollY() const override { return scrollY_; }
    float getAxis(const char* name) const override;

    // IAudioBackend (init/shutdown shared with IInputBackend)
    void play(const char* path, bool loop) override;
    void stop(const char* path) override;
    void stopAll() override;

private:
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_GPUDevice* gpuDevice_;
    SDL_Texture* target3D_;
    bool gpuClaimed_;
    int width_, height_;
    RenderTexture* currentTarget_;
    Color currentFillColor_;
    Color currentStrokeColor_;

    static const int MAX_KEYS = 512;
    static const int MAX_MOUSE = 8;
    bool keysCurr_[MAX_KEYS];
    bool keysPrev_[MAX_KEYS];
    bool mouseCurr_[MAX_MOUSE];
    bool mousePrev_[MAX_MOUSE];
    float mouseX_, mouseY_;
    float mouseDeltaX_, mouseDeltaY_;
    float scrollX_, scrollY_;
    bool inputInitialized_;
    bool audioInitialized_;

    std::unique_ptr<MaterialCache> materialCache_;

    void ensure3DTarget();
};

} // namespace domi

#endif // DOMI_BACKEND_SDL_BACKEND_H
