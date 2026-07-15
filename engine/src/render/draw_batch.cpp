#include "domi/draw_batch.h"

namespace domi {

void DrawBatch::setFillColor(const Color& c) {
    push([c](Canvas2D* canvas) { canvas->setFillColor(c); });
}

void DrawBatch::setStrokeColor(const Color& c) {
    push([c](Canvas2D* canvas) { canvas->setStrokeColor(c); });
}

void DrawBatch::setLineWidth(float w) {
    push([w](Canvas2D* canvas) { canvas->setLineWidth(w); });
}

void DrawBatch::setRenderMode(RenderMode mode) {
    push([mode](Canvas2D* canvas) { canvas->setRenderMode(mode); });
}

void DrawBatch::save() {
    push([](Canvas2D* canvas) { canvas->save(); });
}

void DrawBatch::restore() {
    push([](Canvas2D* canvas) { canvas->restore(); });
}

void DrawBatch::translate(float x, float y) {
    push([x, y](Canvas2D* canvas) { canvas->translate(x, y); });
}

void DrawBatch::rotate(float radians) {
    push([radians](Canvas2D* canvas) { canvas->rotate(radians); });
}

void DrawBatch::scale(float x, float y) {
    push([x, y](Canvas2D* canvas) { canvas->scale(x, y); });
}

void DrawBatch::fillRect(float x, float y, float w, float h) {
    push([x, y, w, h](Canvas2D* canvas) { canvas->fillRect(x, y, w, h); });
}

void DrawBatch::strokeRect(float x, float y, float w, float h) {
    push([x, y, w, h](Canvas2D* canvas) { canvas->strokeRect(x, y, w, h); });
}

void DrawBatch::drawLine(float x1, float y1, float x2, float y2) {
    push([x1, y1, x2, y2](Canvas2D* canvas) { canvas->drawLine(x1, y1, x2, y2); });
}

void DrawBatch::fillCircle(float x, float y, float radius, int segments) {
    push([x, y, radius, segments](Canvas2D* canvas) {
        canvas->fillCircle(x, y, radius, segments);
    });
}

void DrawBatch::beginPath() {
    push([](Canvas2D* canvas) { canvas->beginPath(); });
}

void DrawBatch::moveTo(float x, float y) {
    push([x, y](Canvas2D* canvas) { canvas->moveTo(x, y); });
}

void DrawBatch::lineTo(float x, float y) {
    push([x, y](Canvas2D* canvas) { canvas->lineTo(x, y); });
}

void DrawBatch::closePath() {
    push([](Canvas2D* canvas) { canvas->closePath(); });
}

void DrawBatch::fill() {
    push([](Canvas2D* canvas) { canvas->fill(); });
}

void DrawBatch::stroke() {
    push([](Canvas2D* canvas) { canvas->stroke(); });
}

void DrawBatch::ellipse(float x, float y, float rx, float ry, float rotation,
                        float startAngle, float endAngle, bool ccw) {
    push([x, y, rx, ry, rotation, startAngle, endAngle, ccw](Canvas2D* canvas) {
        canvas->ellipse(x, y, rx, ry, rotation, startAngle, endAngle, ccw);
    });
}

void DrawBatch::drawMaterial(float x, float y, const Material& material) {
    push([x, y, material](Canvas2D* canvas) {
        canvas->drawMaterial(x, y, material);
    });
}

void DrawBatch::begin3D() {
    push([](Canvas2D* canvas) { canvas->begin3D(); });
}

void DrawBatch::end3D() {
    push([](Canvas2D* canvas) { canvas->end3D(); });
}

void DrawBatch::drawMesh3D(float cx, float cy, float scale,
                           float rotX, float rotY, float rotZ,
                           const Mesh3D& mesh) {
    push([cx, cy, scale, rotX, rotY, rotZ, mesh](Canvas2D* canvas) {
        canvas->drawMesh3D(cx, cy, scale, rotX, rotY, rotZ,
                           mesh.vertices.data(), (int)mesh.vertices.size(),
                           mesh.indices.data(), mesh.triangleCount(),
                           mesh.color);
    });
}

void DrawBatch::run(Canvas2D* canvas) const {
    for (size_t i = 0; i < ops_.size(); ++i) {
        ops_[i](canvas);
    }
}

} // namespace domi
