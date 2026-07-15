#ifndef DOMI_RENDER_LIST_H
#define DOMI_RENDER_LIST_H

#include "domi/draw_command.h"
#include "domi/render_layer.h"
#include "domi/material.h"
#include "domi/types.h"
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>

namespace domi {

class Canvas2D;
class Font;
class RenderTexture;

// A declarative deferred 2D draw list.
// Scenes record draw commands with a layer and a z/sort key. The engine commits
// the list exactly once via flush(Canvas2D*), which sorts by layer/z and replays
// the commands. Individual commands never auto-commit.
class RenderList {
public:
    using CustomFn = std::function<void(Canvas2D*)>;

    struct Item {
        RenderLayer layer;
        float z;
        std::unique_ptr<DrawCommand> cmd;
    };

    void clear() { items_.clear(); }

    // Low-level: add a pre-built command.
    void add(RenderLayer layer, float z, std::unique_ptr<DrawCommand> cmd) {
        Item item;
        item.layer = layer;
        item.z = z;
        item.cmd = std::move(cmd);
        items_.push_back(std::move(item));
    }

    // Fallback for complex procedural helpers that are not yet declarative.
    void custom(RenderLayer layer, float z, const CustomFn& fn) {
        add(layer, z, std::unique_ptr<DrawCommand>(new CustomCommand(fn)));
    }

    // Declarative helpers.
    void setFillColor(RenderLayer layer, float z, const Color& c) {
        add(layer, z, std::unique_ptr<DrawCommand>(new SetFillColorCmd(c)));
    }
    void setStrokeColor(RenderLayer layer, float z, const Color& c) {
        add(layer, z, std::unique_ptr<DrawCommand>(new SetStrokeColorCmd(c)));
    }
    void setLineWidth(RenderLayer layer, float z, float w) {
        add(layer, z, std::unique_ptr<DrawCommand>(new SetLineWidthCmd(w)));
    }

    void fillRect(RenderLayer layer, float z, float x, float y, float w, float h) {
        add(layer, z, std::unique_ptr<DrawCommand>(new FillRectCmd(x, y, w, h)));
    }
    void strokeRect(RenderLayer layer, float z, float x, float y, float w, float h) {
        add(layer, z, std::unique_ptr<DrawCommand>(new StrokeRectCmd(x, y, w, h)));
    }
    void drawLine(RenderLayer layer, float z, float x1, float y1, float x2, float y2) {
        add(layer, z, std::unique_ptr<DrawCommand>(new DrawLineCmd(x1, y1, x2, y2)));
    }
    void fillCircle(RenderLayer layer, float z, float x, float y, float radius, int segments = 32) {
        add(layer, z, std::unique_ptr<DrawCommand>(new FillCircleCmd(x, y, radius, segments)));
    }

    void beginPath(RenderLayer layer, float z) {
        add(layer, z, std::unique_ptr<DrawCommand>(new BeginPathCmd()));
    }
    void moveTo(RenderLayer layer, float z, float x, float y) {
        add(layer, z, std::unique_ptr<DrawCommand>(new MoveToCmd(x, y)));
    }
    void lineTo(RenderLayer layer, float z, float x, float y) {
        add(layer, z, std::unique_ptr<DrawCommand>(new LineToCmd(x, y)));
    }
    void closePath(RenderLayer layer, float z) {
        add(layer, z, std::unique_ptr<DrawCommand>(new ClosePathCmd()));
    }
    void fill(RenderLayer layer, float z) {
        add(layer, z, std::unique_ptr<DrawCommand>(new FillCmd()));
    }
    void stroke(RenderLayer layer, float z) {
        add(layer, z, std::unique_ptr<DrawCommand>(new StrokeCmd()));
    }
    void arc(RenderLayer layer, float z, float x, float y, float radius,
             float startAngle, float endAngle, bool ccw = false) {
        add(layer, z, std::unique_ptr<DrawCommand>(new ArcCmd(x, y, radius, startAngle, endAngle, ccw)));
    }
    void arcTo(RenderLayer layer, float z, float x1, float y1, float x2, float y2, float radius) {
        add(layer, z, std::unique_ptr<DrawCommand>(new ArcToCmd(x1, y1, x2, y2, radius)));
    }
    void bezierCurveTo(RenderLayer layer, float z,
                       float cp1x, float cp1y, float cp2x, float cp2y,
                       float x, float y) {
        add(layer, z, std::unique_ptr<DrawCommand>(new BezierCurveToCmd(cp1x, cp1y, cp2x, cp2y, x, y)));
    }
    void quadraticCurveTo(RenderLayer layer, float z, float cpx, float cpy, float x, float y) {
        add(layer, z, std::unique_ptr<DrawCommand>(new QuadraticCurveToCmd(cpx, cpy, x, y)));
    }
    void ellipse(RenderLayer layer, float z, float x, float y,
                 float rx, float ry, float rotation,
                 float startAngle, float endAngle, bool ccw = false) {
        add(layer, z, std::unique_ptr<DrawCommand>(new EllipseCmd(x, y, rx, ry, rotation, startAngle, endAngle, ccw)));
    }

    void drawMaterial(RenderLayer layer, float z, float x, float y, const Material& material) {
        add(layer, z, std::unique_ptr<DrawCommand>(new DrawMaterialCmd(x, y, material)));
    }
    void drawMaterial(RenderLayer layer, float z, float x, float y, const char* key) {
        add(layer, z, std::unique_ptr<DrawCommand>(new DrawMaterialByKeyCmd(x, y, key)));
    }
    void drawText(RenderLayer layer, float z, float x, float y,
                  const char* text, Font* font, const Color& color) {
        add(layer, z, std::unique_ptr<DrawCommand>(new DrawTextCmd(x, y, text, font, color)));
    }
    void drawTexture(RenderLayer layer, float z, float x, float y,
                     RenderTexture* texture, BlendMode mode = BlendMode::Blend) {
        add(layer, z, std::unique_ptr<DrawCommand>(new DrawTextureCmd(x, y, texture, mode)));
    }

    void save(RenderLayer layer, float z) {
        add(layer, z, std::unique_ptr<DrawCommand>(new SaveCmd()));
    }
    void restore(RenderLayer layer, float z) {
        add(layer, z, std::unique_ptr<DrawCommand>(new RestoreCmd()));
    }
    void translate(RenderLayer layer, float z, float x, float y) {
        add(layer, z, std::unique_ptr<DrawCommand>(new TranslateCmd(x, y)));
    }
    void rotate(RenderLayer layer, float z, float radians) {
        add(layer, z, std::unique_ptr<DrawCommand>(new RotateCmd(radians)));
    }
    void scale(RenderLayer layer, float z, float x, float y) {
        add(layer, z, std::unique_ptr<DrawCommand>(new ScaleCmd(x, y)));
    }
    void setTransform(RenderLayer layer, float z,
                      float a, float b, float c, float d, float e, float f) {
        add(layer, z, std::unique_ptr<DrawCommand>(new SetTransformCmd(a, b, c, d, e, f)));
    }
    void resetTransform(RenderLayer layer, float z) {
        add(layer, z, std::unique_ptr<DrawCommand>(new ResetTransformCmd()));
    }

    void setClipRect(RenderLayer layer, float z, float x, float y, float w, float h) {
        add(layer, z, std::unique_ptr<DrawCommand>(new SetClipRectCmd(x, y, w, h)));
    }
    void resetClipRect(RenderLayer layer, float z) {
        add(layer, z, std::unique_ptr<DrawCommand>(new ResetClipRectCmd()));
    }

    // Sort by layer/z and replay all recorded commands onto the canvas.
    void flush(Canvas2D* canvas) {
        std::sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) {
            if (a.layer != b.layer) {
                return static_cast<int>(a.layer) < static_cast<int>(b.layer);
            }
            return a.z < b.z;
        });
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].cmd) {
                items_[i].cmd->execute(canvas);
            }
        }
    }

    size_t size() const { return items_.size(); }

private:
    std::vector<Item> items_;
};

} // namespace domi

#endif // DOMI_RENDER_LIST_H
