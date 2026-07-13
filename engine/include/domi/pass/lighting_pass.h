#ifndef DOMI_LIGHTING_PASS_H
#define DOMI_LIGHTING_PASS_H

#include "domi/render_pass.h"
#include "domi/math.h"

namespace domi {

// Generates a LightBuffer for local point/spot lights.
// The buffer is cleared to ambient light and lights are additively blended on top.
class LightingPass : public RenderPass {
public:
    ~LightingPass() override {}

    void record(CommandBuffer& cmd, RenderContext& ctx) override;

private:
    void drawPointLight(CommandBuffer& cmd, const Vec2& pos, float radius,
                        const Color& color, float intensity);
};

} // namespace domi

#endif // DOMI_LIGHTING_PASS_H
