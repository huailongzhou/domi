#include "domi/pass/ui_pass.h"
#include "domi/render_command_buffer.h"

namespace domi {

void UIPass::record(CommandBuffer& cmd, RenderContext& ctx) {
    (void)ctx;
    // Render directly to the screen after CompositePass.
    cmd.setTarget(NULL);

    // Placeholder: future UI rendering goes here.
}

} // namespace domi
