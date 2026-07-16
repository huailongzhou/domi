#ifndef DOMI_DRAW_BATCH_H
#define DOMI_DRAW_BATCH_H

#include "domi/canvas2d.h"
#include "domi/material.h"
#include "domi/math.h"
#include "3d/mesh3d.h"
#include <functional>
#include <vector>

namespace domi {

// A recorded group of canvas calls that is committed to a RenderList as a
// single item: the whole group shares one z key and replays atomically (in
// recording order, without other items interleaving) when the list flushes.
//
// DrawBatch mirrors the Canvas2D drawing API, but instead of drawing it
// records each call. Scene code builds nodes against this interface and never
// touches Canvas2D directly.
class DrawBatch {
public:
    using Op = std::function<void(Canvas2D*)>;

    bool empty() const { return ops_.empty(); }
    void clear() { ops_.clear(); }

    // Low-level: record a pre-built op.
    void push(Op op) { ops_.push_back(std::move(op)); }

    // Paint state.
    void setFillColor(const Color& c);
    void setStrokeColor(const Color& c);
    void setLineWidth(float w);
    void setRenderMode(RenderMode mode);

    // Transform stack.
    void save();
    void restore();
    void translate(float x, float y);
    void rotate(float radians);
    void scale(float x, float y);

    // Simple shapes.
    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void drawLine(float x1, float y1, float x2, float y2);
    void fillCircle(float x, float y, float radius, int segments = 32);

    // Path API.
    void beginPath();
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void closePath();
    void fill();
    void stroke();
    void ellipse(float x, float y, float rx, float ry, float rotation,
                 float startAngle, float endAngle, bool ccw = false);

    // Materials (referenced, not copied — must outlive run()).
    void drawMaterial(float x, float y, const Material& material);

    // Software-rasterized 3D (mesh referenced, not copied — must outlive run()).
    void begin3D();
    void end3D();
    void drawMesh3D(float cx, float cy, float scale,
                    float rotX, float rotY, float rotZ,
                    const Mesh3D& mesh);

    // Replay all recorded calls against the canvas, in recording order.
    void run(Canvas2D* canvas) const;

private:
    std::vector<Op> ops_;
};

} // namespace domi

#endif // DOMI_DRAW_BATCH_H
