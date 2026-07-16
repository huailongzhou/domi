#include "domi/pass/composite_pass.h"
#include "domi/render_command_buffer.h"
#include "domi/render_texture.h"
#include "domi/camera2d.h"

namespace domi {

void CompositePass::record(CommandBuffer& cmd, RenderContext& ctx) {
    // Render to the default target (screen).
    cmd.setTarget(NULL);

    // 1. Base color.
    if (ctx.colorBuffer && ctx.colorBuffer->valid()) {
        cmd.drawTexture(0, 0, ctx.colorBuffer);
    }

    // 2. Blend shadow mask (darken shadows), clipped to the ground only.
    //    The ground line lives in world space (y = height/3), so it must be
    //    mapped through the 2D camera before clipping in screen space —
    //    otherwise panning the viewport would clip away valid shadows.
    if (ctx.shadowMask && ctx.shadowMask->valid()) {
        float groundY = ctx.height * (1.0f / 3.0f);
        float clipY = groundY;
        if (ctx.camera2D) {
            clipY = groundY * ctx.camera2D->zoom + ctx.camera2D->offsetY;
        }
        if (clipY < 0.0f) clipY = 0.0f;
        if (clipY > (float)ctx.height) clipY = (float)ctx.height;
        cmd.setClipRect(0.0f, clipY, (float)ctx.width, (float)ctx.height - clipY);
        cmd.drawTexture(0, 0, ctx.shadowMask, BlendMode::Blend);
        cmd.resetClipRect();
    }

    // 3. Additive light buffer.
    if (ctx.lightBuffer && ctx.lightBuffer->valid()) {
        cmd.drawTexture(0, 0, ctx.lightBuffer, BlendMode::Add);
    }
}

} // namespace domi
