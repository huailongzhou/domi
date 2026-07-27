#include "domi/render/pass/composite_pass.h"
#include "domi/render/render_texture.h"
#include "domi/render/camera2d.h"
#include "domi/render/canvas2d.h"

namespace domi {

void CompositePass::record(Canvas2D& canvas, RenderContext& ctx) {
    canvas.setRenderTarget(NULL);

    if (ctx.colorBuffer && ctx.colorBuffer->valid()) {
        canvas.drawTexture(0, 0, ctx.colorBuffer);
    }

    if (ctx.shadowMask && ctx.shadowMask->valid()) {
        float groundY = ctx.height * (1.0f / 3.0f);
        float clipY = groundY;
        if (ctx.camera2D) {
            clipY = groundY * ctx.camera2D->zoom + ctx.camera2D->offsetY;
        }
        if (clipY < 0.0f) clipY = 0.0f;
        if (clipY > (float)ctx.height) clipY = (float)ctx.height;
        canvas.setClipRect(0.0f, clipY, (float)ctx.width, (float)ctx.height - clipY);
        canvas.drawTexture(0, 0, ctx.shadowMask, BlendMode::Blend);
        canvas.resetClipRect();
    }

    if (ctx.lightBuffer && ctx.lightBuffer->valid()) {
        canvas.drawTexture(0, 0, ctx.lightBuffer, BlendMode::Add);
    }
}

} // namespace domi
