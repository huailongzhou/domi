#include "domi/pass/geometry_pass.h"
#include "domi/render_command_buffer.h"
#include "domi/render_texture.h"
#include "domi/render_list.h"
#include "domi/scene_manager.h"
#include "domi/camera2d.h"
#include "domi/canvas2d.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include <vector>

namespace domi {

GeometryPass::GeometryPass(SceneManager* sceneManager)
    : sceneManager_(sceneManager) {}

void GeometryPass::record(CommandBuffer& cmd, RenderContext& ctx) {
    if (!ctx.colorBuffer || !ctx.world) return;

    cmd.setTarget(ctx.colorBuffer);

    // Clear to a default sky/ground color. The scene can overwrite this.
    cmd.setFillColor(Color(0.12f, 0.12f, 0.16f, 1.0f));
    cmd.fillRect(0, 0, (float)ctx.width, (float)ctx.height);

    Canvas2D* canvas = cmd.getCanvas();

    // ECS sprites live in world space: draw them under the 2D camera if the
    // active scene provides one.
    bool camActive = canvas && ctx.camera2D;
    if (camActive) {
        canvas->save();
        canvas->translate(ctx.camera2D->offsetX, ctx.camera2D->offsetY);
        canvas->scale(ctx.camera2D->zoom, ctx.camera2D->zoom);
    }

    // Draw ECS sprites.
    std::vector<Entity> entities = ctx.world->queryEntitiesWith(
        ComponentTypeMask().withTransform().withSprite());

    for (size_t i = 0; i < entities.size(); ++i) {
        Entity e = entities[i];
        TransformComponent* t = ctx.world->getComponent<TransformComponent>(e);
        SpriteComponent* s = ctx.world->getComponent<SpriteComponent>(e);
        if (!t || !s) continue;

        // Alpha-zero sprites are used as shadow casters only.
        if (s->color.a <= 0.0f) continue;

        float w = 64.0f * t->transform.scale.x;
        float h = 64.0f * t->transform.scale.y;
        float x = t->transform.position.x - w * 0.5f;
        float y = t->transform.position.y - h * 0.5f;

        cmd.setFillColor(s->color);
        cmd.fillRect(x, y, w, h);
    }

    if (camActive) {
        canvas->restore();
    }

    // Let the active Scene record declarative draw commands on top. World
    // layers (up to Effect) render under the 2D camera; Overlay stays
    // screen-space.
    if (sceneManager_ && canvas) {
        RenderList list;
        sceneManager_->render(list);
        list.flush(canvas, ctx.camera2D, RenderLayer::Effect);
    }
}

} // namespace domi
