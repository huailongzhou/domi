#ifndef DOMI_DRAW_BATCH_H
#define DOMI_DRAW_BATCH_H

#include "domi/render/command_stream.h"
#include "domi/render/canvas2d.h"
#include "domi/render/material.h"
#include "domi/core/math.h"
#include "3d/mesh3d.h"

namespace domi {

// Recorded group of canvas calls, committed to a RenderList as one item.
// Shares CommandStream storage with RenderQueue (different Target type).
class DrawBatch {
public:
    typedef CommandStream<Canvas2D>::Op Op;

    bool empty() const { return stream_.empty(); }
    void clear() { stream_.clear(); }

    void push(Op op) { stream_.push(std::move(op)); }

    void setFillColor(const Color& c);
    void setStrokeColor(const Color& c);
    void setLineWidth(float w);
    void setRenderMode(RenderMode mode);

    void save();
    void restore();
    void translate(float x, float y);
    void rotate(float radians);
    void scale(float x, float y);

    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void drawLine(float x1, float y1, float x2, float y2);
    void fillCircle(float x, float y, float radius, int segments = 32);

    void beginPath();
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void closePath();
    void fill();
    void stroke();
    void ellipse(float x, float y, float rx, float ry, float rotation,
                 float startAngle, float endAngle, bool ccw = false);

    void drawMaterial(float x, float y, const Material& material);

    void begin3D();
    void end3D();
    void drawMesh3D(float cx, float cy, float scale,
                    float rotX, float rotY, float rotZ,
                    const Mesh3D& mesh);

    void run(Canvas2D* canvas) const { stream_.run(canvas); }

private:
    CommandStream<Canvas2D> stream_;
};

} // namespace domi

#endif // DOMI_DRAW_BATCH_H
