#ifndef DOMI_RENDERER_H
#define DOMI_RENDERER_H

#include "domi/render_context.h"
#include "domi/render_texture.h"
#include <vector>

namespace domi {

class Canvas2D;
class CommandBuffer;
class IRenderBackend;
class RenderPass;
class SceneManager;
class Window;
class World;

// Manages a multi-pass 2D rendering pipeline.
//
// Built-in passes render into off-screen RenderTextures and CompositePass
// blends them onto the screen. The Canvas2D + RenderQueue are reused as the
// per-pass drawing backend to keep wasm boundary crossings low.
class Renderer {
public:
    Renderer();
    ~Renderer();

    // Initialize with a render backend and back-buffer size.
    // The Renderer creates and owns its Canvas2D and CommandBuffer.
    bool init(IRenderBackend* backend, int width, int height);

    // Shutdown and release all passes and buffers.
    void shutdown();

    // Execute all passes for one frame and present.
    void render(World* world, SceneManager* sceneManager);

    // Add a pass. Ownership is transferred to Renderer.
    void addPass(RenderPass* pass);

    // Remove all passes without deleting them (caller takes ownership back).
    void clearPasses();

    Canvas2D* getCanvas2D() const { return canvas_; }
    CommandBuffer* getCommandBuffer() const { return cmd_; }

    size_t getPassCount() const { return passes_.size(); }
    RenderPass* getPass(size_t index) const { return index < passes_.size() ? passes_[index] : NULL; }

    RenderTexture* getColorBuffer() { return &colorBuffer_; }
    RenderTexture* getShadowMask() { return &shadowMask_; }
    RenderTexture* getLightBuffer() { return &lightBuffer_; }

    int width() const { return width_; }
    int height() const { return height_; }

private:
    IRenderBackend* backend_;
    Canvas2D* canvas_;
    CommandBuffer* cmd_;
    std::vector<RenderPass*> passes_;

    RenderTexture colorBuffer_;
    RenderTexture shadowMask_;
    RenderTexture lightBuffer_;

    int width_;
    int height_;

    bool createBuffers(int w, int h);
};

} // namespace domi

#endif // DOMI_RENDERER_H
