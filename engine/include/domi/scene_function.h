#ifndef DOMI_SCENE_FUNCTION_H
#define DOMI_SCENE_FUNCTION_H

#include "domi/math.h"

namespace domi {

class Canvas2D;

// A collection of reusable helper functions for 2D scenes.
// These are stateless static utilities so they can be called from scene code
// or from script-driven scenes with minimal overhead.
class SceneFunction {
public:
    // Draw a directional shadow on the ground as a soft ellipse.
    //
    // @param canvas        target canvas
    // @param x, y          sprite center in screen space
    // @param baseRx        shadow radius along the X axis at midday
    // @param baseRy        shadow radius along the Y axis at midday
    // @param baseOffsetY   distance from sprite center to its base (where the
    //                      shadow should be anchored)
    // @param shadowDir     normalized shadow direction (from caster to ground);
    //                      when shadowDir.y <= 0 nothing is drawn
    static void drawShadow(Canvas2D* canvas, float x, float y,
                           float baseRx, float baseRy, float baseOffsetY,
                           const Vec2& shadowDir);
};

} // namespace domi

#endif // DOMI_SCENE_FUNCTION_H
