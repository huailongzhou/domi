#ifndef DOMI_RENDER_PASS_H
#define DOMI_RENDER_PASS_H

#include "domi/render/render_context.h"

namespace domi {

class Renderer;
class Canvas2D;

// Base class for a single rendering stage in the RenderPass pipeline.
class RenderPass {
public:
    virtual ~RenderPass() {}

    virtual void init(Renderer* renderer) { (void)renderer; }
    virtual void resize(int w, int h) { (void)w; (void)h; }

    // Draw into the shared Canvas2D using the frame context.
    virtual void record(Canvas2D& canvas, RenderContext& ctx) = 0;
};

} // namespace domi

#endif // DOMI_RENDER_PASS_H
