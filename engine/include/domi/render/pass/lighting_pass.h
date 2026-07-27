#ifndef DOMI_LIGHTING_PASS_H
#define DOMI_LIGHTING_PASS_H

#include "domi/render/render_pass.h"
#include "domi/core/math.h"

namespace domi {

class Canvas2D;

// Generates a LightBuffer for local point/spot lights.
// The buffer is cleared to ambient light and lights are additively blended on top.
class LightingPass : public RenderPass {
public:
    ~LightingPass() override {}

    void record(Canvas2D& canvas, RenderContext& ctx) override;

private:
    void drawPointLight(Canvas2D& canvas, const Vec2& pos, float radius,
                        const Color& color, float intensity);
};

} // namespace domi

#endif // DOMI_LIGHTING_PASS_H
