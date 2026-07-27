#ifndef DOMI_SHADOW_PASS_H
#define DOMI_SHADOW_PASS_H

#include "domi/render/render_pass.h"
#include "domi/core/math.h"

namespace domi {

class Canvas2D;

// Generates a ShadowMask from directional light occluders.
// White means fully lit, darker values modulate the final color in CompositePass.
class ShadowPass : public RenderPass {
public:
    ~ShadowPass() override {}

    void record(Canvas2D& canvas, RenderContext& ctx) override;

private:
    void drawOccluderShadow(Canvas2D& canvas, const Vec2& pos, float radius,
                            const Vec2& shadowDir, float groundY);
};

} // namespace domi

#endif // DOMI_SHADOW_PASS_H
