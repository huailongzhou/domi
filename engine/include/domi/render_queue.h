#ifndef DOMI_RENDER_QUEUE_H
#define DOMI_RENDER_QUEUE_H

#include "domi/render_command.h"
#include "domi/math.h"
#include <vector>

namespace domi {

class IRenderBackend;

// A lightweight queue of 2D render commands.
//
// When the renderer is driven from WebAssembly, recording draw calls into a
// queue and flushing them once per frame greatly reduces wasm-to-native
// boundary crossings compared to immediate-mode rendering.
class RenderQueue {
public:
    RenderQueue();
    ~RenderQueue();

    void clear();
    bool empty() const { return commands_.empty(); }

    void setFillColor(const Color& c);
    void setStrokeColor(const Color& c);
    void setLineWidth(float w);

    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void clearRect(float x, float y, float w, float h);
    void drawLine(float x1, float y1, float x2, float y2);
    void fillCircle(float x, float y, float radius, int segments);

    void fillPath(const std::vector<Vec2>& points, bool closed);
    void strokePath(const std::vector<Vec2>& points, bool closed);

    // Enqueue a material draw using a backend-cached texture handle.
    // The handle must have been obtained from IRenderBackend::uploadMaterial().
    // Rotation is in radians around (centerX, centerY) relative to (x, y).
    void drawMaterial(float x, float y, void* handle,
                      float angle, float centerX, float centerY,
                      float scaleX, float scaleY);

    // Execute all queued commands against the backend and clear the queue.
    void flush(IRenderBackend* backend);

private:
    std::vector<RenderCommand> commands_;
    std::vector<StoredPath> paths_;

    Color currentFillColor_;
    Color currentStrokeColor_;
    float currentLineWidth_;

    void pushPathCommand(RenderCommand::Type type,
                         const std::vector<Vec2>& points, bool closed);
    static void renderPath(IRenderBackend* backend, const StoredPath& path,
                           const Color& color, bool fill, float lineWidth);
};

} // namespace domi

#endif // DOMI_RENDER_QUEUE_H
