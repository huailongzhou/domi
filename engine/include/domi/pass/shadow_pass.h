#ifndef DOMI_SHADOW_PASS_H
#define DOMI_SHADOW_PASS_H

#include "domi/render_pass.h"
#include "domi/math.h"

namespace domi {

// Generates a ShadowMask from directional light occluders.
// White means fully lit, darker values modulate the final color in CompositePass.
class ShadowPass : public RenderPass {
public:
    ~ShadowPass() override {}

    void record(CommandBuffer& cmd, RenderContext& ctx) override;

private:
    void drawOccluderShadow(CommandBuffer& cmd, const Vec2& pos, float radius,
                            const Vec2& shadowDir, float groundY);
};

} // namespace domi

#endif // DOMI_SHADOW_PASS_H
