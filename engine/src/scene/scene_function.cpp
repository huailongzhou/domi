#include "domi/scene_function.h"
#include "domi/draw_batch.h"

namespace domi {

void SceneFunction::drawShadow(DrawBatch& batch, float x, float y,
                               float baseRx, float baseRy, float baseOffsetY,
                               const Vec2& shadowDir) {
    if (shadowDir.y <= 0.0f) return;

    // Stretch the shadow horizontally as the sun gets lower.
    float lengthScale = 1.0f + (1.0f - shadowDir.y) * 0.8f;
    float horizontalOffset = shadowDir.x * (baseRx * 1.5f / shadowDir.y);
    Vec2 shadowPos(x + horizontalOffset, y + baseOffsetY);

    float rx = baseRx * lengthScale;
    float ry = baseRy;

    batch.setFillColor(Color(0.0f, 0.0f, 0.0f, 0.45f));
    batch.beginPath();
    batch.ellipse(shadowPos.x, shadowPos.y, rx, ry,
                  0.0f, 0.0f, 2.0f * 3.14159265f);
    batch.fill();
}

} // namespace domi
