#include "domi/render/pass/geometry_pass.h"
#include "domi/render/render_texture.h"
#include "domi/render/render_list.h"
#include "domi/scene/scene_manager.h"
#include "domi/render/camera2d.h"
#include "domi/render/canvas2d.h"
#include "domi/ecs/ecs.h"
#include "domi/ecs/component.h"
#include <vector>

namespace domi {

GeometryPass::GeometryPass(SceneManager* sceneManager)
    : sceneManager_(sceneManager) {}

void GeometryPass::record(Canvas2D& canvas, RenderContext& ctx) {
    if (!ctx.colorBuffer || !ctx.world) return;

    canvas.setRenderTarget(ctx.colorBuffer);

    canvas.setFillColor(Color(0.12f, 0.12f, 0.16f, 1.0f));
    canvas.fillRect(0, 0, (float)ctx.width, (float)ctx.height);

    bool camActive = ctx.camera2D != NULL;
    if (camActive) {
        canvas.save();
        canvas.translate(ctx.camera2D->offsetX, ctx.camera2D->offsetY);
        canvas.scale(ctx.camera2D->zoom, ctx.camera2D->zoom);
    }

    std::vector<Entity> entities = ctx.world->queryEntitiesWith(
        ComponentTypeMask().withTransform().withSprite());

    for (size_t i = 0; i < entities.size(); ++i) {
        Entity e = entities[i];
        TransformComponent* t = ctx.world->getComponent<TransformComponent>(e);
        SpriteComponent* s = ctx.world->getComponent<SpriteComponent>(e);
        if (!t || !s) continue;

        if (s->color.a <= 0.0f) continue;

        float w = 64.0f * t->transform.scale.x;
        float h = 64.0f * t->transform.scale.y;
        float x = t->transform.position.x - w * 0.5f;
        float y = t->transform.position.y - h * 0.5f;

        canvas.setFillColor(s->color);
        canvas.fillRect(x, y, w, h);
    }

    if (camActive) {
        canvas.restore();
    }

    if (sceneManager_) {
        RenderList list;
        sceneManager_->render(list);
        list.flush(&canvas, ctx.camera2D, RenderLayer::Effect);
    }
}

} // namespace domi
