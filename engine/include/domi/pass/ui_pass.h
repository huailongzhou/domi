#ifndef DOMI_UI_PASS_H
#define DOMI_UI_PASS_H

#include "domi/render_pass.h"

namespace domi {

class ClayUI;

// Renders UI on top of the final composited image, unaffected by lighting.
// Handles both the legacy UIView tree and the Clay-based UI declared by the
// active scene via Scene::buildClayUI().
class UIPass : public RenderPass {
public:
    UIPass() : clayUI_(NULL) {}
    ~UIPass() override;

    void record(CommandBuffer& cmd, RenderContext& ctx) override;

private:
    // Raw pointer (ClayUI is incomplete here); owned and freed in ui_pass.cpp.
    ClayUI* clayUI_;
};

} // namespace domi

#endif // DOMI_UI_PASS_H
