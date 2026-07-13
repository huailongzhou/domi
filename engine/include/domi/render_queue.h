#ifndef DOMI_RENDER_QUEUE_H
#define DOMI_RENDER_QUEUE_H

#include "domi/render_command.h"
#include <vector>

struct SDL_Renderer;

namespace domi {

// A lightweight queue of 2D render commands.
//
// When the renderer is driven from WebAssembly, recording draw calls into a
// queue and flushing them once per frame greatly reduces wasm-to-native
// boundary crossings compared to immediate-mode SDL_Renderer calls.
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

    // Execute all queued commands against the SDL renderer and clear the queue.
    void flush(SDL_Renderer* renderer);

private:
    std::vector<RenderCommand> commands_;
    std::vector<StoredPath> paths_;

    Color currentFillColor_;
    Color currentStrokeColor_;
    float currentLineWidth_;

    void pushPathCommand(RenderCommand::Type type,
                         const std::vector<Vec2>& points, bool closed);
    static void applyColor(SDL_Renderer* renderer, const Color& c);
    static void renderPath(SDL_Renderer* renderer, const StoredPath& path,
                           const Color& color, bool fill, float lineWidth);
};

} // namespace domi

#endif // DOMI_RENDER_QUEUE_H
