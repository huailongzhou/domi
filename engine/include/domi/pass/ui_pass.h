#ifndef DOMI_UI_PASS_H
#define DOMI_UI_PASS_H

#include "domi/render_pass.h"

namespace domi {

// Renders UI on top of the final composited image, unaffected by lighting.
// Currently a placeholder; Scene UI can be hooked here in the future.
class UIPass : public RenderPass {
public:
    ~UIPass() override {}

    void record(CommandBuffer& cmd, RenderContext& ctx) override;
};

} // namespace domi

#endif // DOMI_UI_PASS_H
