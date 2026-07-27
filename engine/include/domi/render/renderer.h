#ifndef DOMI_RENDERER_H
#define DOMI_RENDERER_H

#include "domi/render/render_context.h"
#include "domi/render/render_texture.h"
#include <functional>
#include <vector>

namespace domi {

class Canvas2D;
class IRenderBackend;
class RenderPass;
class SceneManager;
class Window;
class World;

// Manages a multi-pass 2D rendering pipeline.
//
// Built-in passes render into off-screen RenderTextures and CompositePass
// blends them onto the screen. Canvas2D is the shared drawing surface.
class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(IRenderBackend* backend, int width, int height);
    void shutdown();

    void render(World* world, SceneManager* sceneManager, float fps = 0.0f);

    void addPass(RenderPass* pass);
    void clearPasses();

    Canvas2D* getCanvas2D() const { return canvas_; }

    size_t getPassCount() const { return passes_.size(); }
    RenderPass* getPass(size_t index) const { return index < passes_.size() ? passes_[index] : NULL; }

    RenderTexture* getColorBuffer() { return &colorBuffer_; }
    RenderTexture* getShadowMask() { return &shadowMask_; }
    RenderTexture* getLightBuffer() { return &lightBuffer_; }

    int width() const { return width_; }
    int height() const { return height_; }

    void setPrePresentHook(std::function<void()> hook) {
        prePresentHook_ = std::move(hook);
    }

private:
    IRenderBackend* backend_;
    Canvas2D* canvas_;
    std::vector<RenderPass*> passes_;

    RenderTexture colorBuffer_;
    RenderTexture shadowMask_;
    RenderTexture lightBuffer_;

    int width_;
    int height_;

    std::function<void()> prePresentHook_;

    bool createBuffers(int w, int h);
};

} // namespace domi

#endif // DOMI_RENDERER_H
