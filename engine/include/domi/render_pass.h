#ifndef DOMI_RENDER_PASS_H
#define DOMI_RENDER_PASS_H

#include "domi/render_context.h"

namespace domi {

class Renderer;
class CommandBuffer;

// Base class for a single rendering stage in the RenderPass pipeline.
class RenderPass {
public:
    virtual ~RenderPass() {}

    virtual void init(Renderer* renderer) { (void)renderer; }
    virtual void resize(int w, int h) { (void)w; (void)h; }

    // Record draw commands into cmd using the shared context.
    virtual void record(CommandBuffer& cmd, RenderContext& ctx) = 0;
};

} // namespace domi

#endif // DOMI_RENDER_PASS_H
