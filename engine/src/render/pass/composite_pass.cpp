#include "domi/pass/composite_pass.h"
#include "domi/render_command_buffer.h"
#include "domi/render_texture.h"
#include "domi/canvas2d.h"
#include <SDL3/SDL.h>

namespace domi {

void CompositePass::record(CommandBuffer& cmd, RenderContext& ctx) {
    Canvas2D* canvas = cmd.getCanvas();
    if (!canvas) return;

    // Render to the default target (screen).
    cmd.setTarget(NULL);

    // 1. Base color.
    if (ctx.colorBuffer && ctx.colorBuffer->valid()) {
        canvas->drawTexture(0, 0, ctx.colorBuffer);
    }

    // 2. Blend shadow mask (darken shadows), clipped to the ground only.
    if (ctx.shadowMask && ctx.shadowMask->valid()) {
        SDL_Renderer* renderer = canvas->getRenderer();
        if (renderer) {
            float groundY = ctx.height * (1.0f / 3.0f);
            cmd.setClipRect(0.0f, groundY, (float)ctx.width, (float)ctx.height - groundY);
            SDL_SetTextureBlendMode(ctx.shadowMask->getNative(), SDL_BLENDMODE_BLEND);
            canvas->drawTexture(0, 0, ctx.shadowMask);
            cmd.resetClipRect();
        }
    }

    // 3. Additive light buffer.
    if (ctx.lightBuffer && ctx.lightBuffer->valid()) {
        SDL_Renderer* renderer = canvas->getRenderer();
        if (renderer) {
            SDL_SetTextureBlendMode(ctx.lightBuffer->getNative(), SDL_BLENDMODE_ADD);
            canvas->drawTexture(0, 0, ctx.lightBuffer);
            SDL_SetTextureBlendMode(ctx.lightBuffer->getNative(), SDL_BLENDMODE_BLEND);
        }
    }
}

} // namespace domi
