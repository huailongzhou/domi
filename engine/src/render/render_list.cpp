#include "domi/render_list.h"
#include "domi/canvas2d.h"
#include "domi/draw_batch.h"
#include "domi/render_texture.h"
#include <algorithm>
#include <string>

namespace domi {

void RenderList::submit(RenderLayer layer, float z, const DrawBatch& batch) {
    add(layer, z, [batch](Canvas2D* canvas) { batch.run(canvas); });
}

void RenderList::setFillColor(RenderLayer layer, float z, const Color& c) {
    add(layer, z, [c](Canvas2D* canvas) { canvas->setFillColor(c); });
}

void RenderList::setStrokeColor(RenderLayer layer, float z, const Color& c) {
    add(layer, z, [c](Canvas2D* canvas) { canvas->setStrokeColor(c); });
}

void RenderList::setLineWidth(RenderLayer layer, float z, float w) {
    add(layer, z, [w](Canvas2D* canvas) { canvas->setLineWidth(w); });
}

void RenderList::fillRect(RenderLayer layer, float z, float x, float y, float w, float h) {
    add(layer, z, [x, y, w, h](Canvas2D* canvas) { canvas->fillRect(x, y, w, h); });
}

void RenderList::strokeRect(RenderLayer layer, float z, float x, float y, float w, float h) {
    add(layer, z, [x, y, w, h](Canvas2D* canvas) { canvas->strokeRect(x, y, w, h); });
}

void RenderList::drawLine(RenderLayer layer, float z, float x1, float y1, float x2, float y2) {
    add(layer, z, [x1, y1, x2, y2](Canvas2D* canvas) { canvas->drawLine(x1, y1, x2, y2); });
}

void RenderList::fillCircle(RenderLayer layer, float z, float x, float y, float radius, int segments) {
    add(layer, z, [x, y, radius, segments](Canvas2D* canvas) {
        canvas->fillCircle(x, y, radius, segments);
    });
}

void RenderList::beginPath(RenderLayer layer, float z) {
    add(layer, z, [](Canvas2D* canvas) { canvas->beginPath(); });
}

void RenderList::moveTo(RenderLayer layer, float z, float x, float y) {
    add(layer, z, [x, y](Canvas2D* canvas) { canvas->moveTo(x, y); });
}

void RenderList::lineTo(RenderLayer layer, float z, float x, float y) {
    add(layer, z, [x, y](Canvas2D* canvas) { canvas->lineTo(x, y); });
}

void RenderList::closePath(RenderLayer layer, float z) {
    add(layer, z, [](Canvas2D* canvas) { canvas->closePath(); });
}

void RenderList::fill(RenderLayer layer, float z) {
    add(layer, z, [](Canvas2D* canvas) { canvas->fill(); });
}

void RenderList::stroke(RenderLayer layer, float z) {
    add(layer, z, [](Canvas2D* canvas) { canvas->stroke(); });
}

void RenderList::arc(RenderLayer layer, float z, float x, float y, float radius,
                     float startAngle, float endAngle, bool ccw) {
    add(layer, z, [x, y, radius, startAngle, endAngle, ccw](Canvas2D* canvas) {
        canvas->arc(x, y, radius, startAngle, endAngle, ccw);
    });
}

void RenderList::arcTo(RenderLayer layer, float z, float x1, float y1, float x2, float y2, float radius) {
    add(layer, z, [x1, y1, x2, y2, radius](Canvas2D* canvas) {
        canvas->arcTo(x1, y1, x2, y2, radius);
    });
}

void RenderList::bezierCurveTo(RenderLayer layer, float z,
                               float cp1x, float cp1y, float cp2x, float cp2y,
                               float x, float y) {
    add(layer, z, [cp1x, cp1y, cp2x, cp2y, x, y](Canvas2D* canvas) {
        canvas->bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
    });
}

void RenderList::quadraticCurveTo(RenderLayer layer, float z, float cpx, float cpy, float x, float y) {
    add(layer, z, [cpx, cpy, x, y](Canvas2D* canvas) {
        canvas->quadraticCurveTo(cpx, cpy, x, y);
    });
}

void RenderList::ellipse(RenderLayer layer, float z, float x, float y,
                         float rx, float ry, float rotation,
                         float startAngle, float endAngle, bool ccw) {
    add(layer, z, [x, y, rx, ry, rotation, startAngle, endAngle, ccw](Canvas2D* canvas) {
        canvas->ellipse(x, y, rx, ry, rotation, startAngle, endAngle, ccw);
    });
}

void RenderList::drawMaterial(RenderLayer layer, float z, float x, float y, const Material& material) {
    add(layer, z, [x, y, material](Canvas2D* canvas) {
        canvas->drawMaterial(x, y, material);
    });
}

void RenderList::drawMaterial(RenderLayer layer, float z, float x, float y, const char* key) {
    const std::string keyStr = key ? key : "";
    add(layer, z, [x, y, keyStr](Canvas2D* canvas) {
        canvas->drawMaterial(keyStr.c_str(), x, y);
    });
}

void RenderList::drawText(RenderLayer layer, float z, float x, float y,
                          const char* text, Font* font, const Color& color) {
    const std::string str = text ? text : "";
    add(layer, z, [x, y, str, font, color](Canvas2D* canvas) {
        if (font) canvas->drawText(x, y, str.c_str(), font, color);
    });
}

void RenderList::drawTexture(RenderLayer layer, float z, float x, float y,
                             RenderTexture* texture, BlendMode mode) {
    add(layer, z, [x, y, texture, mode](Canvas2D* canvas) {
        canvas->drawTexture(x, y, texture, mode);
    });
}

void RenderList::save(RenderLayer layer, float z) {
    add(layer, z, [](Canvas2D* canvas) { canvas->save(); });
}

void RenderList::restore(RenderLayer layer, float z) {
    add(layer, z, [](Canvas2D* canvas) { canvas->restore(); });
}

void RenderList::translate(RenderLayer layer, float z, float x, float y) {
    add(layer, z, [x, y](Canvas2D* canvas) { canvas->translate(x, y); });
}

void RenderList::rotate(RenderLayer layer, float z, float radians) {
    add(layer, z, [radians](Canvas2D* canvas) { canvas->rotate(radians); });
}

void RenderList::scale(RenderLayer layer, float z, float x, float y) {
    add(layer, z, [x, y](Canvas2D* canvas) { canvas->scale(x, y); });
}

void RenderList::setTransform(RenderLayer layer, float z,
                              float a, float b, float c, float d, float e, float f) {
    add(layer, z, [a, b, c, d, e, f](Canvas2D* canvas) {
        canvas->setTransform(a, b, c, d, e, f);
    });
}

void RenderList::resetTransform(RenderLayer layer, float z) {
    add(layer, z, [](Canvas2D* canvas) { canvas->resetTransform(); });
}

void RenderList::setClipRect(RenderLayer layer, float z, float x, float y, float w, float h) {
    add(layer, z, [x, y, w, h](Canvas2D* canvas) { canvas->setClipRect(x, y, w, h); });
}

void RenderList::resetClipRect(RenderLayer layer, float z) {
    add(layer, z, [](Canvas2D* canvas) { canvas->resetClipRect(); });
}

void RenderList::flush(Canvas2D* canvas) {
    std::stable_sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) {
        if (a.layer != b.layer) {
            return static_cast<int>(a.layer) < static_cast<int>(b.layer);
        }
        return a.z < b.z;
    });
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].fn) {
            items_[i].fn(canvas);
        }
    }
}

} // namespace domi
