#include "domi/render/pass/voxel_pass.h"
#include "domi/render/render_texture.h"
#include "domi/render/render_context.h"
#include "domi/render/canvas2d.h"
#include "domi/ecs/ecs.h"
#include "domi/ecs/component.h"
#include "voxel_renderer.h"
#include <cstddef>

namespace domi {

namespace {

const TransformComponent* findCameraTransform(World* world, const CameraComponent* camera) {
    if (!world || !camera) return NULL;
    std::vector<Entity> cameras = world->queryEntitiesWith(
        ComponentTypeMask().withCamera().withTransform());
    for (size_t i = 0; i < cameras.size(); ++i) {
        CameraComponent* cam = world->getComponent<CameraComponent>(cameras[i]);
        if (cam == camera) {
            return world->getComponent<TransformComponent>(cameras[i]);
        }
    }
    return NULL;
}

} // anonymous namespace

Voxel3DPass::Voxel3DPass()
    : renderer_(NULL) {}

Voxel3DPass::~Voxel3DPass() {
    delete renderer_;
}

void Voxel3DPass::record(Canvas2D& canvas, RenderContext& ctx) {
    if (!ctx.colorBuffer || !ctx.world) return;

    std::vector<Entity> entities = ctx.world->queryEntitiesWith(
        ComponentTypeMask().withTransform().withVoxel());
    if (entities.empty()) return;

    canvas.setRenderTarget(ctx.colorBuffer);

    if (!renderer_) {
        renderer_ = new VoxelRenderer(&canvas);
    }

    const TransformComponent* camTransform = findCameraTransform(ctx.world, ctx.camera);

    canvas.save();
    canvas.resetTransform();

    renderer_->render(ctx.world, ctx.camera, camTransform, ctx.width, ctx.height);

    canvas.restore();
}

} // namespace domi
