#include "domi/scene_function.h"
#include "domi/canvas2d.h"
#include <cmath>

namespace domi {

void SceneFunction::drawShadow(Canvas2D* canvas, float x, float y,
                               float baseRx, float baseRy, float baseOffsetY,
                               const Vec2& shadowDir) {
    if (!canvas || shadowDir.y <= 0.0f) return;

    // Stretch the shadow horizontally as the sun gets lower.
    float lengthScale = 1.0f + (1.0f - shadowDir.y) * 0.8f;
    float horizontalOffset = shadowDir.x * (baseRx * 1.5f / shadowDir.y);
    Vec2 shadowPos(x + horizontalOffset, y + baseOffsetY);

    float rx = baseRx * lengthScale;
    float ry = baseRy;

    canvas->setFillColor(Color(0.0f, 0.0f, 0.0f, 0.45f));
    canvas->beginPath();
    int segments = 20;
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * 3.14159265f;
        float px = shadowPos.x + std::cos(angle) * rx;
        float py = shadowPos.y + std::sin(angle) * ry;
        if (i == 0) canvas->moveTo(px, py);
        else canvas->lineTo(px, py);
    }
    canvas->closePath();
    canvas->fill();
}

} // namespace domi
