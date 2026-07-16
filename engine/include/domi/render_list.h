#ifndef DOMI_RENDER_LIST_H
#define DOMI_RENDER_LIST_H

#include "domi/render_layer.h"
#include "domi/material.h"
#include "domi/math.h"
#include "domi/types.h"
#include <functional>
#include <vector>

namespace domi {

class Canvas2D;
class DrawBatch;
class Font;
class RenderTexture;
struct Camera2D;

// A declarative deferred 2D draw list.
// Scenes record draw calls with a layer and a z/sort key. The engine commits
// the list exactly once via flush(Canvas2D*), which sorts by layer/z and
// replays the recorded calls. Individual calls never auto-commit.
//
// Recorded calls are plain std::function callbacks — the same mechanism
// RenderQueue uses one level lower, so the engine has a single command model.
class RenderList {
public:
    using CustomFn = std::function<void(Canvas2D*)>;

    struct Item {
        RenderLayer layer;
        float z;
        CustomFn fn;
    };

    void clear() { items_.clear(); }

    // Low-level: record a pre-built callback.
    void add(RenderLayer layer, float z, CustomFn fn) {
        Item item;
        item.layer = layer;
        item.z = z;
        item.fn = std::move(fn);
        items_.push_back(std::move(item));
    }

    // Submit a recorded batch of canvas calls as a single item: the whole
    // batch shares the given z and replays atomically at flush time.
    void submit(RenderLayer layer, float z, const DrawBatch& batch);

    // Declarative helpers.
    void setFillColor(RenderLayer layer, float z, const Color& c);
    void setStrokeColor(RenderLayer layer, float z, const Color& c);
    void setLineWidth(RenderLayer layer, float z, float w);

    void fillRect(RenderLayer layer, float z, float x, float y, float w, float h);
    void strokeRect(RenderLayer layer, float z, float x, float y, float w, float h);
    void drawLine(RenderLayer layer, float z, float x1, float y1, float x2, float y2);
    void fillCircle(RenderLayer layer, float z, float x, float y, float radius, int segments = 32);

    void beginPath(RenderLayer layer, float z);
    void moveTo(RenderLayer layer, float z, float x, float y);
    void lineTo(RenderLayer layer, float z, float x, float y);
    void closePath(RenderLayer layer, float z);
    void fill(RenderLayer layer, float z);
    void stroke(RenderLayer layer, float z);
    void arc(RenderLayer layer, float z, float x, float y, float radius,
             float startAngle, float endAngle, bool ccw = false);
    void arcTo(RenderLayer layer, float z, float x1, float y1, float x2, float y2, float radius);
    void bezierCurveTo(RenderLayer layer, float z,
                       float cp1x, float cp1y, float cp2x, float cp2y,
                       float x, float y);
    void quadraticCurveTo(RenderLayer layer, float z, float cpx, float cpy, float x, float y);
    void ellipse(RenderLayer layer, float z, float x, float y,
                 float rx, float ry, float rotation,
                 float startAngle, float endAngle, bool ccw = false);

    void drawMaterial(RenderLayer layer, float z, float x, float y, const Material& material);
    void drawMaterial(RenderLayer layer, float z, float x, float y, const char* key);
    void drawText(RenderLayer layer, float z, float x, float y,
                  const char* text, Font* font, const Color& color);
    void drawTexture(RenderLayer layer, float z, float x, float y,
                     RenderTexture* texture, BlendMode mode = BlendMode::Blend);

    void save(RenderLayer layer, float z);
    void restore(RenderLayer layer, float z);
    void translate(RenderLayer layer, float z, float x, float y);
    void rotate(RenderLayer layer, float z, float radians);
    void scale(RenderLayer layer, float z, float x, float y);
    void setTransform(RenderLayer layer, float z,
                      float a, float b, float c, float d, float e, float f);
    void resetTransform(RenderLayer layer, float z);

    void setClipRect(RenderLayer layer, float z, float x, float y, float w, float h);
    void resetClipRect(RenderLayer layer, float z);

    // Sort by layer/z and replay all recorded calls onto the canvas.
    // stable_sort preserves insertion order for calls that share the same
    // layer and z, which matters for state changes (fill/stroke colors) that
    // must execute in the order they were recorded.
    void flush(Canvas2D* canvas);

    // Same as flush(), but items in layers up to worldUpTo (inclusive) are
    // drawn under the given 2D camera transform; later layers (Overlay, UI)
    // stay in screen space. Passing NULL behaves like flush(canvas).
    void flush(Canvas2D* canvas, const Camera2D* camera, RenderLayer worldUpTo);

    size_t size() const { return items_.size(); }

private:
    std::vector<Item> items_;
};

} // namespace domi

#endif // DOMI_RENDER_LIST_H
