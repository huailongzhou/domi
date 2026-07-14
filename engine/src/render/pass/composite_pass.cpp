#include "domi/pass/composite_pass.h"
#include "domi/render_command_buffer.h"
#include "domi/render_texture.h"

namespace domi {

void CompositePass::record(CommandBuffer& cmd, RenderContext& ctx) {
    // Render to the default target (screen).
    cmd.setTarget(NULL);

    // 1. Base color.
    if (ctx.colorBuffer && ctx.colorBuffer->valid()) {
        cmd.drawTexture(0, 0, ctx.colorBuffer);
    }

    // 2. Blend shadow mask (darken shadows), clipped to the ground only.
    if (ctx.shadowMask && ctx.shadowMask->valid()) {
        float groundY = ctx.height * (1.0f / 3.0f);
        cmd.setClipRect(0.0f, groundY, (float)ctx.width, (float)ctx.height - groundY);
        cmd.drawTexture(0, 0, ctx.shadowMask, BlendMode::Blend);
        cmd.resetClipRect();
    }

    // 3. Additive light buffer.
    if (ctx.lightBuffer && ctx.lightBuffer->valid()) {
        cmd.drawTexture(0, 0, ctx.lightBuffer, BlendMode::Add);
    }
}

} // namespace domi
