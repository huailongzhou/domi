#include "domi/pass/lighting_pass.h"
#include "domi/render_command_buffer.h"
#include "domi/render_texture.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include <vector>
#include <cmath>

namespace domi {

void LightingPass::record(CommandBuffer& cmd, RenderContext& ctx) {
    if (!ctx.lightBuffer) return;

    cmd.setTarget(ctx.lightBuffer);

    // Clear to ambient light level (dark blue-grey).
    cmd.setFillColor(Color(0.15f, 0.15f, 0.22f, 1.0f));
    cmd.fillRect(0, 0, (float)ctx.width, (float)ctx.height);

    if (!ctx.world) return;

    // Additively draw point lights.
    std::vector<Entity> entities = ctx.world->queryEntitiesWith(
        ComponentTypeMask().withLight().withTransform());

    for (size_t i = 0; i < entities.size(); ++i) {
        LightComponent* l = ctx.world->getComponent<LightComponent>(entities[i]);
        TransformComponent* t = ctx.world->getComponent<TransformComponent>(entities[i]);
        if (!l || !t) continue;
        if (l->type != LightComponent::Point) continue;

        Vec2 pos(t->transform.position.x, t->transform.position.y);
        float radius = 120.0f * t->transform.scale.x;
        drawPointLight(cmd, pos, radius, l->color, l->intensity);
    }
}

void LightingPass::drawPointLight(CommandBuffer& cmd, const Vec2& pos,
                                  float radius, const Color& color, float intensity) {
    // Approximate a radial gradient with a few concentric circles.
    // Inner circle is bright, outer rings fade out.
    int rings = 6;
    for (int r = rings; r >= 0; --r) {
        float t = (float)r / (float)rings;
        float rradius = radius * (1.0f - t * 0.85f);
        float alpha = intensity * (1.0f - t);
        cmd.setFillColor(Color(color.r, color.g, color.b, alpha));
        cmd.fillCircle(pos.x, pos.y, rradius, 32);
    }
}

} // namespace domi
