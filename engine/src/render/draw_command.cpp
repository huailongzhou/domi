#include "domi/draw_command.h"
#include "domi/canvas2d.h"
#include "domi/ui/font.h"
#include "domi/render_texture.h"

namespace domi {

void SetFillColorCmd::execute(Canvas2D* canvas) const { canvas->setFillColor(color_); }
void SetStrokeColorCmd::execute(Canvas2D* canvas) const { canvas->setStrokeColor(color_); }
void SetLineWidthCmd::execute(Canvas2D* canvas) const { canvas->setLineWidth(width_); }

void FillRectCmd::execute(Canvas2D* canvas) const { canvas->fillRect(x_, y_, w_, h_); }
void StrokeRectCmd::execute(Canvas2D* canvas) const { canvas->strokeRect(x_, y_, w_, h_); }
void DrawLineCmd::execute(Canvas2D* canvas) const { canvas->drawLine(x1_, y1_, x2_, y2_); }
void FillCircleCmd::execute(Canvas2D* canvas) const { canvas->fillCircle(x_, y_, radius_, segments_); }

void BeginPathCmd::execute(Canvas2D* canvas) const { canvas->beginPath(); }
void MoveToCmd::execute(Canvas2D* canvas) const { canvas->moveTo(x_, y_); }
void LineToCmd::execute(Canvas2D* canvas) const { canvas->lineTo(x_, y_); }
void ClosePathCmd::execute(Canvas2D* canvas) const { canvas->closePath(); }
void FillCmd::execute(Canvas2D* canvas) const { canvas->fill(); }
void StrokeCmd::execute(Canvas2D* canvas) const { canvas->stroke(); }

void ArcCmd::execute(Canvas2D* canvas) const {
    canvas->arc(x_, y_, radius_, startAngle_, endAngle_, ccw_);
}

void ArcToCmd::execute(Canvas2D* canvas) const {
    canvas->arcTo(x1_, y1_, x2_, y2_, radius_);
}

void BezierCurveToCmd::execute(Canvas2D* canvas) const {
    canvas->bezierCurveTo(cp1x_, cp1y_, cp2x_, cp2y_, x_, y_);
}

void QuadraticCurveToCmd::execute(Canvas2D* canvas) const {
    canvas->quadraticCurveTo(cpx_, cpy_, x_, y_);
}

void EllipseCmd::execute(Canvas2D* canvas) const {
    canvas->ellipse(x_, y_, rx_, ry_, rotation_, startAngle_, endAngle_, ccw_);
}

void DrawMaterialCmd::execute(Canvas2D* canvas) const {
    canvas->drawMaterial(x_, y_, material_);
}

void DrawMaterialByKeyCmd::execute(Canvas2D* canvas) const {
    canvas->drawMaterial(key_.c_str(), x_, y_);
}

void DrawTextCmd::execute(Canvas2D* canvas) const {
    if (font_) canvas->drawText(x_, y_, text_.c_str(), font_, color_);
}

void DrawTextureCmd::execute(Canvas2D* canvas) const {
    canvas->drawTexture(x_, y_, texture_, mode_);
}

void SaveCmd::execute(Canvas2D* canvas) const { canvas->save(); }
void RestoreCmd::execute(Canvas2D* canvas) const { canvas->restore(); }
void TranslateCmd::execute(Canvas2D* canvas) const { canvas->translate(x_, y_); }
void RotateCmd::execute(Canvas2D* canvas) const { canvas->rotate(radians_); }
void ScaleCmd::execute(Canvas2D* canvas) const { canvas->scale(x_, y_); }
void SetTransformCmd::execute(Canvas2D* canvas) const { canvas->setTransform(a_, b_, c_, d_, e_, f_); }
void ResetTransformCmd::execute(Canvas2D* canvas) const { canvas->resetTransform(); }

void SetClipRectCmd::execute(Canvas2D* canvas) const { canvas->setClipRect(x_, y_, w_, h_); }
void ResetClipRectCmd::execute(Canvas2D* canvas) const { canvas->resetClipRect(); }

void CustomCommand::execute(Canvas2D* canvas) const {
    if (fn_) fn_(canvas);
}

} // namespace domi
