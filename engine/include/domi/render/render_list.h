#ifndef DOMI_RENDER_LIST_H
#define DOMI_RENDER_LIST_H

#include "domi/render/command_stream.h"
#include "domi/render/render_layer.h"
#include "domi/render/material.h"
#include "domi/core/math.h"
#include "domi/core/types.h"

namespace domi {

class Canvas2D;
class DrawBatch;
class Font;
class RenderTexture;
struct Camera2D;

// Sort key for deferred 2D draws: primary layer, secondary z.
struct RenderSortKey {
    RenderLayer layer;
    float z;

    RenderSortKey() : layer(RenderLayer::Background), z(0.0f) {}
    RenderSortKey(RenderLayer l, float z_) : layer(l), z(z_) {}

    bool operator<(const RenderSortKey& o) const {
        if (layer != o.layer) {
            return static_cast<int>(layer) < static_cast<int>(o.layer);
        }
        return z < o.z;
    }
};

// Declarative deferred 2D draw list (scene / pass level).
// Built on SortedCommandStream; flush sorts by layer/z then replays on Canvas2D.
class RenderList {
public:
    typedef CommandStream<Canvas2D>::Op CustomFn;
    typedef SortedCommandStream<Canvas2D, RenderSortKey>::Item Item;

    void clear() { stream_.clear(); }

    void add(RenderLayer layer, float z, CustomFn fn) {
        stream_.add(RenderSortKey(layer, z), std::move(fn));
    }

    void submit(RenderLayer layer, float z, const DrawBatch& batch);

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

    void flush(Canvas2D* canvas);
    void flush(Canvas2D* canvas, const Camera2D* camera, RenderLayer worldUpTo);

    size_t size() const { return stream_.size(); }

private:
    SortedCommandStream<Canvas2D, RenderSortKey> stream_;
};

} // namespace domi

#endif // DOMI_RENDER_LIST_H
