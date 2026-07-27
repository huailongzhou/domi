#include "domi/render/pass/shadow_pass.h"
#include "domi/render/render_texture.h"
#include "domi/render/camera2d.h"
#include "domi/render/canvas2d.h"
#include "domi/ecs/ecs.h"
#include "domi/ecs/component.h"
#include <vector>
#include <cmath>

namespace domi {

void ShadowPass::record(Canvas2D& canvas, RenderContext& ctx) {
    if (!ctx.shadowMask) return;

    canvas.setRenderTarget(ctx.shadowMask);
    canvas.clear(Color(1.0f, 1.0f, 1.0f, 0.0f));

    Vec2 lightDir(0.3f, -0.7f);
    if (ctx.sun && ctx.world) {
        std::vector<Entity> lights = ctx.world->queryEntitiesWith(
            ComponentTypeMask().withLight().withTransform());
        for (size_t i = 0; i < lights.size(); ++i) {
            LightComponent* l = ctx.world->getComponent<LightComponent>(lights[i]);
            TransformComponent* t = ctx.world->getComponent<TransformComponent>(lights[i]);
            if (l == ctx.sun && t) {
                Vec3 fwd = t->transform.forward();
                lightDir = Vec2(fwd.x, fwd.y).normalized();
                if (lightDir.length() < 0.001f) lightDir = Vec2(0.3f, -0.7f);
                break;
            }
        }
    }

    Vec2 shadowDir = lightDir * -1.0f;
    float groundY = ctx.height * (1.0f / 3.0f);

    if (ctx.world) {
        bool camActive = ctx.camera2D != NULL;
        if (camActive) {
            canvas.save();
            canvas.translate(ctx.camera2D->offsetX, ctx.camera2D->offsetY);
            canvas.scale(ctx.camera2D->zoom, ctx.camera2D->zoom);
        }

        std::vector<Entity> entities = ctx.world->queryEntitiesWith(
            ComponentTypeMask().withTransform().withSprite());
        for (size_t i = 0; i < entities.size(); ++i) {
            SpriteComponent* s = ctx.world->getComponent<SpriteComponent>(entities[i]);
            TransformComponent* t = ctx.world->getComponent<TransformComponent>(entities[i]);
            if (!s || !s->castShadow || !t) continue;
            if (s->color.a > 0.0f) continue;

            Vec2 pos(t->transform.position.x, t->transform.position.y);
            float radius = 40.0f * t->transform.scale.x;
            drawOccluderShadow(canvas, pos, radius, shadowDir, groundY);
        }

        if (camActive) {
            canvas.restore();
        }
    }
}

void ShadowPass::drawOccluderShadow(Canvas2D& canvas, const Vec2& pos,
                                    float radius, const Vec2& shadowDir,
                                    float groundY) {
    if (shadowDir.y <= 0.0f) return;

    float t = (groundY - pos.y) / shadowDir.y;
    Vec2 groundPos(pos.x + shadowDir.x * t, groundY);

    float spread = 1.0f + t * 0.002f;
    float rx = radius * spread;
    float ry = radius * 0.35f * spread;

    canvas.setFillColor(Color(0.0f, 0.0f, 0.0f, 0.6f));
    canvas.beginPath();
    int segments = 20;
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * 3.14159265f;
        float x = groundPos.x + std::cos(angle) * rx;
        float y = groundPos.y + std::sin(angle) * ry;
        if (i == 0) canvas.moveTo(x, y);
        else canvas.lineTo(x, y);
    }
    canvas.closePath();
    canvas.fill();
}

} // namespace domi
