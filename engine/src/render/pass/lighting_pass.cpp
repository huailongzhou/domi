#include "domi/render/pass/lighting_pass.h"
#include "domi/render/render_texture.h"
#include "domi/render/camera2d.h"
#include "domi/render/canvas2d.h"
#include "domi/ecs/ecs.h"
#include "domi/ecs/component.h"
#include <vector>
#include <cmath>

namespace domi {

void LightingPass::record(Canvas2D& canvas, RenderContext& ctx) {
    if (!ctx.lightBuffer) return;

    canvas.setRenderTarget(ctx.lightBuffer);

    canvas.setFillColor(Color(0.15f, 0.15f, 0.22f, 1.0f));
    canvas.fillRect(0, 0, (float)ctx.width, (float)ctx.height);

    if (!ctx.world) return;

    bool camActive = ctx.camera2D != NULL;
    if (camActive) {
        canvas.save();
        canvas.translate(ctx.camera2D->offsetX, ctx.camera2D->offsetY);
        canvas.scale(ctx.camera2D->zoom, ctx.camera2D->zoom);
    }

    std::vector<Entity> entities = ctx.world->queryEntitiesWith(
        ComponentTypeMask().withLight().withTransform());

    for (size_t i = 0; i < entities.size(); ++i) {
        LightComponent* l = ctx.world->getComponent<LightComponent>(entities[i]);
        TransformComponent* t = ctx.world->getComponent<TransformComponent>(entities[i]);
        if (!l || !t) continue;
        if (l->type != LightComponent::Point) continue;

        Vec2 pos(t->transform.position.x, t->transform.position.y);
        float radius = 120.0f * t->transform.scale.x;
        drawPointLight(canvas, pos, radius, l->color, l->intensity);
    }

    if (camActive) {
        canvas.restore();
    }
}

void LightingPass::drawPointLight(Canvas2D& canvas, const Vec2& pos,
                                  float radius, const Color& color, float intensity) {
    int rings = 6;
    for (int r = rings; r >= 0; --r) {
        float t = (float)r / (float)rings;
        float rradius = radius * (1.0f - t * 0.85f);
        float alpha = intensity * (1.0f - t);
        canvas.setFillColor(Color(color.r, color.g, color.b, alpha));
        canvas.fillCircle(pos.x, pos.y, rradius, 32);
    }
}

} // namespace domi
