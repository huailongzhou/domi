#ifndef DOMI_RENDER_H
#define DOMI_RENDER_H

#include "domi/math.h"
#include "domi/ecs.h"
#include <SDL3/SDL.h>

namespace domi {

class Window;
class SceneManager;
class Canvas2D;
class Renderer;

class RenderSystem {
public:
    RenderSystem(Window* window);
    ~RenderSystem();

    bool init();
    void shutdown();

    void render(World* world, SceneManager* sceneManager = NULL);

    Canvas2D* getCanvas2D() { return canvas_; }

    // 2D
    void drawSprite(const Vec3& pos, const Vec2& size, const Color& color, const char* texture);

    // 3D
    void drawMesh(const Mat4& model, const char* meshPath, const char* materialPath);

    void setCamera(const Mat4& view, const Mat4& projection);

    int getWidth() const;
    int getHeight() const;

private:
    Window* window_;
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
