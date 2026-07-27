#ifndef DOMI_RENDER_QUEUE_H
#define DOMI_RENDER_QUEUE_H

#include "domi/render/command_stream.h"
#include "domi/core/math.h"
#include <vector>

namespace domi {

class IRenderBackend;

// Backend-level command stream used by Canvas2D.
// Records IRenderBackend ops; flush() replays in submission order.
class RenderQueue {
public:
    RenderQueue();
    ~RenderQueue();

    typedef CommandStream<IRenderBackend>::Op BackendOp;

    void clear() { stream_.clear(); }
    bool empty() const { return stream_.empty(); }

    void setFillColor(const Color& c) { currentFillColor_ = c; }
    void setStrokeColor(const Color& c) { currentStrokeColor_ = c; }
    void setLineWidth(float w) { currentLineWidth_ = w; }

    void fillPath(const std::vector<Vec2>& points, bool closed);
    void strokePath(const std::vector<Vec2>& points, bool closed);
    void drawLine(float x1, float y1, float x2, float y2);

    void drawMaterial(float x, float y, void* handle,
                      float angle, float centerX, float centerY,
                      float scaleX, float scaleY);

    void drawMaterialRegion(float x, float y, void* handle,
                            int srcX, int srcY, int srcW, int srcH,
                            const Color& tint,
                            float angle, float centerX, float centerY,
                            float scaleX, float scaleY);

    void push(BackendOp op) { stream_.push(std::move(op)); }

    void flush(IRenderBackend* backend) { stream_.flush(backend); }

private:
    CommandStream<IRenderBackend> stream_;

    Color currentFillColor_;
    Color currentStrokeColor_;
    float currentLineWidth_;
};

} // namespace domi

#endif // DOMI_RENDER_QUEUE_H
