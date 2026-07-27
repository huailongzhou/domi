#include "domi/render/renderer.h"
#include "domi/backend/render_backend.h"
#include "domi/render/canvas2d.h"
#include "domi/render/render_pass.h"
#include "domi/scene/scene_manager.h"
#include "domi/scene/scene.h"
#include "domi/ecs/ecs.h"
#include "domi/ecs/component.h"
#include <cstdio>

namespace domi {

namespace {

CameraComponent* findActiveCamera(World* world) {
    if (!world) return NULL;
    std::vector<Entity> cameras = world->queryEntitiesWith(
        ComponentTypeMask().withCamera().withTransform());
    for (size_t i = 0; i < cameras.size(); ++i) {
        CameraComponent* cam = world->getComponent<CameraComponent>(cameras[i]);
        if (cam && cam->isActive) return cam;
    }
    return NULL;
}

LightComponent* findDirectionalLight(World* world) {
    if (!world) return NULL;
    std::vector<Entity> lights = world->queryEntitiesWith(
        ComponentTypeMask().withLight().withTransform());
    for (size_t i = 0; i < lights.size(); ++i) {
        LightComponent* light = world->getComponent<LightComponent>(lights[i]);
        if (light && light->type == LightComponent::Directional) return light;
    }
    return NULL;
}

} // anonymous namespace

Renderer::Renderer()
    : backend_(NULL), canvas_(NULL),
      width_(0), height_(0) {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(IRenderBackend* backend, int width, int height) {
    if (!backend || width <= 0 || height <= 0) return false;

    shutdown();

    backend_ = backend;
    canvas_ = new Canvas2D(backend_);

    if (!createBuffers(width, height)) {
        shutdown();
        return false;
    }

    width_ = width;
    height_ = height;
    return true;
}

void Renderer::shutdown() {
    for (size_t i = 0; i < passes_.size(); ++i) {
        delete passes_[i];
    }
    passes_.clear();

    colorBuffer_.destroy();
    shadowMask_.destroy();
    lightBuffer_.destroy();

    delete canvas_;
    canvas_ = NULL;

    backend_ = NULL;
    width_ = 0;
    height_ = 0;
}

bool Renderer::createBuffers(int w, int h) {
    if (!colorBuffer_.create(backend_, w, h, RenderTextureFormat::RGBA8888)) {
        fprintf(stderr, "[RENDERER] Failed to create color buffer\n");
        return false;
    }
    if (!shadowMask_.create(backend_, w, h, RenderTextureFormat::RGBA8888)) {
        fprintf(stderr, "[RENDERER] Failed to create shadow mask\n");
        return false;
    }
    if (!lightBuffer_.create(backend_, w, h, RenderTextureFormat::RGBA8888)) {
        fprintf(stderr, "[RENDERER] Failed to create light buffer\n");
        return false;
    }
    return true;
}

void Renderer::render(World* world, SceneManager* sceneManager, float fps) {
    if (!backend_ || !canvas_) return;

    canvas_->setRenderTarget(NULL);
    backend_->clear(Color(0.0f, 0.0f, 0.0f, 1.0f));

    Scene* scene = sceneManager ? sceneManager->current() : NULL;

    RenderContext ctx;
    ctx.colorBuffer = &colorBuffer_;
    ctx.shadowMask = &shadowMask_;
    ctx.lightBuffer = &lightBuffer_;
    ctx.finalBuffer = NULL;
    ctx.world = world;
    ctx.scene = scene;
    ctx.camera = findActiveCamera(world);
    ctx.sun = findDirectionalLight(world);
    ctx.uiRoot = scene ? scene->getUIRoot() : NULL;
    ctx.uiContext = scene ? scene->getUIContext() : NULL;
    ctx.camera2D = scene ? scene->getCamera2D() : NULL;
    ctx.fps = fps;
    ctx.width = width_;
    ctx.height = height_;

    for (size_t i = 0; i < passes_.size(); ++i) {
        passes_[i]->record(*canvas_, ctx);
    }

    canvas_->flush();
    if (prePresentHook_) prePresentHook_();
    backend_->present();
}

void Renderer::addPass(RenderPass* pass) {
    if (pass) passes_.push_back(pass);
}

void Renderer::clearPasses() {
    passes_.clear();
}

} // namespace domi
