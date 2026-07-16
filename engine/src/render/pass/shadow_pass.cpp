#include "domi/pass/shadow_pass.h"
#include "domi/render_command_buffer.h"
#include "domi/render_texture.h"
#include "domi/camera2d.h"
#include "domi/canvas2d.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include <vector>
#include <cmath>

namespace domi {

void ShadowPass::record(CommandBuffer& cmd, RenderContext& ctx) {
    if (!ctx.shadowMask) return;

    cmd.setTarget(ctx.shadowMask);

    // Clear the shadow mask to transparent white. clear() ignores the renderer's
    // blend mode so the target is fully cleared even when alpha blending is on.
    cmd.clear(Color(1.0f, 1.0f, 1.0f, 0.0f));

    // Default light direction if no sun is present.
    Vec2 lightDir(0.3f, -0.7f);
    if (ctx.sun && ctx.world) {
        // Use the sun entity's forward direction projected to 2D.
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

    // Cast opposite to light direction.
    Vec2 shadowDir = lightDir * -1.0f;

    // Ground starts at 1/3 of the screen height; the upper 2/3 is grass.
    float groundY = ctx.height * (1.0f / 3.0f);

    // Project a shadow from each shadow-casting sprite onto the ground.
    // The projection happens in world space, so it renders under the same
    // 2D camera as the geometry pass.
    if (ctx.world) {
        Canvas2D* canvas = cmd.getCanvas();
        bool camActive = canvas && ctx.camera2D;
        if (camActive) {
            canvas->save();
            canvas->translate(ctx.camera2D->offsetX, ctx.camera2D->offsetY);
            canvas->scale(ctx.camera2D->zoom, ctx.camera2D->zoom);
        }

        std::vector<Entity> entities = ctx.world->queryEntitiesWith(
            ComponentTypeMask().withTransform().withSprite());
        for (size_t i = 0; i < entities.size(); ++i) {
            SpriteComponent* s = ctx.world->getComponent<SpriteComponent>(entities[i]);
            TransformComponent* t = ctx.world->getComponent<TransformComponent>(entities[i]);
            if (!s || !s->castShadow || !t) continue;
            // Alpha-zero sprites are shadow casters only (clouds); skip sprites
            // that have visible geometry (ground objects draw their own shadows
            // in the scene's render order so they don't overlap the caster).
            if (s->color.a > 0.0f) continue;

            Vec2 pos(t->transform.position.x, t->transform.position.y);
            float radius = 40.0f * t->transform.scale.x;
            drawOccluderShadow(cmd, pos, radius, shadowDir, groundY);
        }

        if (camActive) {
            canvas->restore();
        }
    }
}

void ShadowPass::drawOccluderShadow(CommandBuffer& cmd, const Vec2& pos,
                                    float radius, const Vec2& shadowDir,
                                    float groundY) {
    // Only draw if the shadow direction reaches the ground below the caster.
    // Whether the caster is occluded by the horizon is determined by its
    // SpriteComponent::castShadow flag, set per-frame by the scene.
    if (shadowDir.y <= 0.0f) return;

    // Project the caster down along the shadow direction until it hits groundY.
    float t = (groundY - pos.y) / shadowDir.y;
    Vec2 groundPos(pos.x + shadowDir.x * t, groundY);

    // Ellipse grows and softens with distance.
    float spread = 1.0f + t * 0.002f;
    float rx = radius * spread;
    float ry = radius * 0.35f * spread;

    // Darken the grass where the cloud shadow lands.
    cmd.setFillColor(Color(0.0f, 0.0f, 0.0f, 0.6f));
    cmd.beginPath();
    int segments = 20;
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * 3.14159265f;
        float x = groundPos.x + std::cos(angle) * rx;
        float y = groundPos.y + std::sin(angle) * ry;
        if (i == 0) cmd.moveTo(x, y);
        else cmd.lineTo(x, y);
    }
    cmd.closePath();
    cmd.fill();
}

} // namespace domi
