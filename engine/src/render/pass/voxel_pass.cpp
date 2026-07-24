#include "domi/pass/voxel_pass.h"
#include "domi/render_command_buffer.h"
#include "domi/render_texture.h"
#include "domi/render_context.h"
#include "domi/canvas2d.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "voxel_renderer.h"
#include <cstddef>

namespace domi {

namespace {

// Find the transform component that belongs to the active camera.
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

void Voxel3DPass::record(CommandBuffer& cmd, RenderContext& ctx) {
    if (!ctx.colorBuffer || !ctx.world) return;

    // Only enter the 3D block if there is something to draw.
    std::vector<Entity> entities = ctx.world->queryEntitiesWith(
        ComponentTypeMask().withTransform().withVoxel());
    if (entities.empty()) return;

    cmd.setTarget(ctx.colorBuffer);

    Canvas2D* canvas = cmd.getCanvas();
    if (!canvas) return;

    if (!renderer_) {
        renderer_ = new VoxelRenderer(canvas);
    }

    const TransformComponent* camTransform = findCameraTransform(ctx.world, ctx.camera);

    // The voxel renderer produces world-space geometry and should not be
    // affected by any 2D camera transform that previous passes left on the
    // canvas stack. Faces are sorted back-to-front and drawn as 2D paths,
    // so we do not use the CPU 3D target/depth-buffer block here.
    canvas->save();
    canvas->resetTransform();

    renderer_->render(ctx.world, ctx.camera, camTransform, ctx.width, ctx.height);

    canvas->restore();
}

} // namespace domi
