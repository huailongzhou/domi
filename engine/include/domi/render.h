#ifndef DOMI_RENDER_H
#define DOMI_RENDER_H

#include "domi/math.h"
#include "domi/ecs.h"

struct SDL_GPUBuffer;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUTexture;

namespace domi {

class IRenderBackend;
class IWindowBackend;
class SceneManager;
class Canvas2D;
class Renderer;

class RenderSystem {
public:
    RenderSystem(IRenderBackend* renderBackend, IWindowBackend* windowBackend);
    ~RenderSystem();

    bool init();
    void shutdown();

    void render(World* world, SceneManager* sceneManager = NULL, float fps = 0.0f);

    Canvas2D* getCanvas2D() { return canvas_; }

    // 2D
    void drawSprite(const Vec3& pos, const Vec2& size, const Color& color, const char* texture);

    // 3D
    void drawMesh(const Mat4& model, const char* meshPath, const char* materialPath);

    void setCamera(const Mat4& view, const Mat4& projection);

    int getWidth() const;
    int getHeight() const;

private:
    IRenderBackend* renderBackend_;
    IWindowBackend* windowBackend_;
    bool useGPU_;
    Canvas2D* canvas_;
    Renderer* renderer_;

    // 3D GPU state
    bool gpu3DInited_;
    SDL_GPUBuffer* cubeVertexBuffer_;
    SDL_GPUBuffer* dynamicVertexBuffer_;
    SDL_GPUGraphicsPipeline* cubePipeline_;
    SDL_GPUTexture* depthTexture_;
    int drawableW_;
    int drawableH_;
    float angleX_, angleY_, angleZ_;
    size_t dynamicVertexCapacity_;

    void render2D(World* world);
    void render3D(World* world);
    bool initGPU3D();
    void shutdownGPU3D();
    bool createDepthTexture(int w, int h);
};

} // namespace domi

#endif
