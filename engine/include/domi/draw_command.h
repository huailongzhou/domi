#ifndef DOMI_DRAW_COMMAND_H
#define DOMI_DRAW_COMMAND_H

#include "domi/math.h"
#include "domi/material.h"
#include "domi/types.h"
#include <functional>
#include <string>
#include <vector>

namespace domi {

class Canvas2D;
class Font;
class RenderTexture;

// Base class for declarative 2D draw commands.
// A Scene records commands into a RenderList; the engine commits them later
// by calling execute(Canvas2D*) in sorted order.
class DrawCommand {
public:
    virtual ~DrawCommand() {}
    virtual void execute(Canvas2D* canvas) const = 0;
};

// ------------------------------------------------------------------
// State commands
// ------------------------------------------------------------------

class SetFillColorCmd : public DrawCommand {
public:
    explicit SetFillColorCmd(const Color& c) : color_(c) {}
    void execute(Canvas2D* canvas) const override;
private:
    Color color_;
};

class SetStrokeColorCmd : public DrawCommand {
public:
    explicit SetStrokeColorCmd(const Color& c) : color_(c) {}
    void execute(Canvas2D* canvas) const override;
private:
    Color color_;
};

class SetLineWidthCmd : public DrawCommand {
public:
    explicit SetLineWidthCmd(float w) : width_(w) {}
    void execute(Canvas2D* canvas) const override;
private:
    float width_;
};

// ------------------------------------------------------------------
// Simple shapes
// ------------------------------------------------------------------

class FillRectCmd : public DrawCommand {
public:
    FillRectCmd(float x, float y, float w, float h) : x_(x), y_(y), w_(w), h_(h) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_, w_, h_;
};

class StrokeRectCmd : public DrawCommand {
public:
    StrokeRectCmd(float x, float y, float w, float h) : x_(x), y_(y), w_(w), h_(h) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_, w_, h_;
};

class DrawLineCmd : public DrawCommand {
public:
    DrawLineCmd(float x1, float y1, float x2, float y2)
        : x1_(x1), y1_(y1), x2_(x2), y2_(y2) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x1_, y1_, x2_, y2_;
};

class FillCircleCmd : public DrawCommand {
public:
    FillCircleCmd(float x, float y, float radius, int segments)
        : x_(x), y_(y), radius_(radius), segments_(segments) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_, radius_;
    int segments_;
};

// ------------------------------------------------------------------
// Path commands
// ------------------------------------------------------------------

class BeginPathCmd : public DrawCommand {
public:
    void execute(Canvas2D* canvas) const override;
};

class MoveToCmd : public DrawCommand {
public:
    MoveToCmd(float x, float y) : x_(x), y_(y) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_;
};

class LineToCmd : public DrawCommand {
public:
    LineToCmd(float x, float y) : x_(x), y_(y) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_;
};

class ClosePathCmd : public DrawCommand {
public:
    void execute(Canvas2D* canvas) const override;
};

class FillCmd : public DrawCommand {
public:
    void execute(Canvas2D* canvas) const override;
};

class StrokeCmd : public DrawCommand {
public:
    void execute(Canvas2D* canvas) const override;
};

class ArcCmd : public DrawCommand {
public:
    ArcCmd(float x, float y, float radius, float startAngle, float endAngle, bool ccw)
        : x_(x), y_(y), radius_(radius), startAngle_(startAngle), endAngle_(endAngle), ccw_(ccw) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_, radius_, startAngle_, endAngle_;
    bool ccw_;
};

class ArcToCmd : public DrawCommand {
public:
    ArcToCmd(float x1, float y1, float x2, float y2, float radius)
        : x1_(x1), y1_(y1), x2_(x2), y2_(y2), radius_(radius) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x1_, y1_, x2_, y2_, radius_;
};

class BezierCurveToCmd : public DrawCommand {
public:
    BezierCurveToCmd(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
        : cp1x_(cp1x), cp1y_(cp1y), cp2x_(cp2x), cp2y_(cp2y), x_(x), y_(y) {}
    void execute(Canvas2D* canvas) const override;
private:
    float cp1x_, cp1y_, cp2x_, cp2y_, x_, y_;
};

class QuadraticCurveToCmd : public DrawCommand {
public:
    QuadraticCurveToCmd(float cpx, float cpy, float x, float y)
        : cpx_(cpx), cpy_(cpy), x_(x), y_(y) {}
    void execute(Canvas2D* canvas) const override;
private:
    float cpx_, cpy_, x_, y_;
};

class EllipseCmd : public DrawCommand {
public:
    EllipseCmd(float x, float y, float rx, float ry, float rotation,
               float startAngle, float endAngle, bool ccw)
        : x_(x), y_(y), rx_(rx), ry_(ry), rotation_(rotation),
          startAngle_(startAngle), endAngle_(endAngle), ccw_(ccw) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_, rx_, ry_, rotation_, startAngle_, endAngle_;
    bool ccw_;
};

// ------------------------------------------------------------------
// Materials, textures and text
// ------------------------------------------------------------------

class DrawMaterialCmd : public DrawCommand {
public:
    DrawMaterialCmd(float x, float y, const Material& material)
        : x_(x), y_(y), material_(material) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_;
    Material material_;
};

class DrawMaterialByKeyCmd : public DrawCommand {
public:
    DrawMaterialByKeyCmd(float x, float y, const std::string& key)
        : x_(x), y_(y), key_(key) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_;
    std::string key_;
};

class DrawTextCmd : public DrawCommand {
public:
    DrawTextCmd(float x, float y, const std::string& text, Font* font, const Color& color)
        : x_(x), y_(y), text_(text), font_(font), color_(color) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_;
    std::string text_;
    Font* font_;
    Color color_;
};

class DrawTextureCmd : public DrawCommand {
public:
    DrawTextureCmd(float x, float y, RenderTexture* texture, BlendMode mode)
        : x_(x), y_(y), texture_(texture), mode_(mode) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_;
    RenderTexture* texture_;
    BlendMode mode_;
};

// ------------------------------------------------------------------
// Transform and state stack
// ------------------------------------------------------------------

class SaveCmd : public DrawCommand {
public:
    void execute(Canvas2D* canvas) const override;
};

class RestoreCmd : public DrawCommand {
public:
    void execute(Canvas2D* canvas) const override;
};

class TranslateCmd : public DrawCommand {
public:
    TranslateCmd(float x, float y) : x_(x), y_(y) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_;
};

class RotateCmd : public DrawCommand {
public:
    explicit RotateCmd(float radians) : radians_(radians) {}
    void execute(Canvas2D* canvas) const override;
private:
    float radians_;
};

class ScaleCmd : public DrawCommand {
public:
    ScaleCmd(float x, float y) : x_(x), y_(y) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_;
};

class SetTransformCmd : public DrawCommand {
public:
    SetTransformCmd(float a, float b, float c, float d, float e, float f)
        : a_(a), b_(b), c_(c), d_(d), e_(e), f_(f) {}
    void execute(Canvas2D* canvas) const override;
private:
    float a_, b_, c_, d_, e_, f_;
};

class ResetTransformCmd : public DrawCommand {
public:
    void execute(Canvas2D* canvas) const override;
};

// ------------------------------------------------------------------
// Clip and target
// ------------------------------------------------------------------

class SetClipRectCmd : public DrawCommand {
public:
    SetClipRectCmd(float x, float y, float w, float h)
        : x_(x), y_(y), w_(w), h_(h) {}
    void execute(Canvas2D* canvas) const override;
private:
    float x_, y_, w_, h_;
};

class ResetClipRectCmd : public DrawCommand {
public:
    void execute(Canvas2D* canvas) const override;
};

// ------------------------------------------------------------------
// Custom fallback
// ------------------------------------------------------------------

class CustomCommand : public DrawCommand {
public:
    using DrawFn = std::function<void(Canvas2D*)>;
    explicit CustomCommand(DrawFn fn) : fn_(fn) {}
    void execute(Canvas2D* canvas) const override;
private:
    DrawFn fn_;
};

} // namespace domi

#endif // DOMI_DRAW_COMMAND_H
