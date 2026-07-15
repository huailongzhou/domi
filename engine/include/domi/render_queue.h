#ifndef DOMI_RENDER_QUEUE_H
#define DOMI_RENDER_QUEUE_H

#include "domi/math.h"
#include <functional>
#include <vector>

namespace domi {

class IRenderBackend;

// A lightweight queue of recorded backend operations.
//
// Canvas2D bakes its high-level primitives (paths, lines, materials) into
// backend calls and records them here as plain callbacks. flush() replays
// them against the backend in submission order. This is the same
// record-a-callback mechanism that RenderList uses one level higher, so the
// engine has a single command model instead of parallel command vocabularies.
class RenderQueue {
public:
    RenderQueue();
    ~RenderQueue();

    using BackendOp = std::function<void(IRenderBackend*)>;

    void clear();
    bool empty() const { return ops_.empty(); }

    // Record-time paint state. Captured by the ops recorded afterwards.
    void setFillColor(const Color& c) { currentFillColor_ = c; }
    void setStrokeColor(const Color& c) { currentStrokeColor_ = c; }
    void setLineWidth(float w) { currentLineWidth_ = w; }

    void fillPath(const std::vector<Vec2>& points, bool closed);
    void strokePath(const std::vector<Vec2>& points, bool closed);
    void drawLine(float x1, float y1, float x2, float y2);

    // Enqueue a material draw using a backend-cached texture handle.
    // The handle must have been obtained from IRenderBackend::uploadMaterial().
    // Rotation is in radians around (centerX, centerY) relative to (x, y).
    void drawMaterial(float x, float y, void* handle,
                      float angle, float centerX, float centerY,
                      float scaleX, float scaleY);

    // Low-level: record a pre-built backend operation.
    void push(BackendOp op) { ops_.push_back(std::move(op)); }

    // Execute all queued operations against the backend and clear the queue.
    void flush(IRenderBackend* backend);

private:
    std::vector<BackendOp> ops_;

    Color currentFillColor_;
    Color currentStrokeColor_;
    float currentLineWidth_;
};

} // namespace domi

#endif // DOMI_RENDER_QUEUE_H
