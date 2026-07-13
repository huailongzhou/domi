#ifndef DOMI_COMPOSITE_PASS_H
#define DOMI_COMPOSITE_PASS_H

#include "domi/render_pass.h"

namespace domi {

// Composites colorBuffer, shadowMask and lightBuffer into the final image.
// Renders directly to the default target (screen).
class CompositePass : public RenderPass {
public:
    ~CompositePass() override {}

    void record(CommandBuffer& cmd, RenderContext& ctx) override;
};

} // namespace domi

#endif // DOMI_COMPOSITE_PASS_H
