#include "domi/renderer.h"
#include "domi/canvas2d.h"
#include "domi/render_command_buffer.h"
#include "domi/render_pass.h"
#include "domi/scene_manager.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include <SDL3/SDL.h>
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
    : renderer_(NULL), canvas_(NULL), cmd_(NULL),
      width_(0), height_(0) {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(SDL_Renderer* renderer, int width, int height) {
    if (!renderer || width <= 0 || height <= 0) return false;

    shutdown();

    renderer_ = renderer;
    canvas_ = new Canvas2D(renderer_);
    cmd_ = new CommandBuffer(canvas_);

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

    delete cmd_;
    cmd_ = NULL;
    delete canvas_;
    canvas_ = NULL;

    renderer_ = NULL;
    width_ = 0;
    height_ = 0;
}

bool Renderer::createBuffers(int w, int h) {
    if (!colorBuffer_.create(renderer_, w, h, SDL_PIXELFORMAT_RGBA8888)) {
        fprintf(stderr, "[RENDERER] Failed to create color buffer\n");
        return false;
    }
    if (!shadowMask_.create(renderer_, w, h, SDL_PIXELFORMAT_RGBA8888)) {
        fprintf(stderr, "[RENDERER] Failed to create shadow mask\n");
        return false;
    }
    if (!lightBuffer_.create(renderer_, w, h, SDL_PIXELFORMAT_RGBA8888)) {
        fprintf(stderr, "[RENDERER] Failed to create light buffer\n");
        return false;
    }
    return true;
}

void Renderer::render(World* world, SceneManager* sceneManager) {
    if (!renderer_ || !canvas_ || !cmd_) return;

    // Ensure we start on the default target with a clean state.
    canvas_->setRenderTarget(NULL);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    RenderContext ctx;
    ctx.colorBuffer = &colorBuffer_;
    ctx.shadowMask = &shadowMask_;
    ctx.lightBuffer = &lightBuffer_;
    ctx.finalBuffer = NULL; // final target is the screen
    ctx.world = world;
    ctx.camera = findActiveCamera(world);
    ctx.sun = findDirectionalLight(world);
    ctx.width = width_;
    ctx.height = height_;

    for (size_t i = 0; i < passes_.size(); ++i) {
        passes_[i]->record(*cmd_, ctx);
    }

    // Submit remaining queued commands and present.
    canvas_->flush();
    SDL_RenderPresent(renderer_);
}

void Renderer::addPass(RenderPass* pass) {
    if (pass) passes_.push_back(pass);
}

void Renderer::clearPasses() {
    passes_.clear();
}

} // namespace domi
