#include "rock_generator.h"

#include <algorithm>

namespace domi {

Material RockGenerator::build() {
    std::vector<uint8_t> rgba(width_ * height_ * 4, 0);

    int cx = width_ / 2;
    int cy = height_ / 2 + rockRadius_ / 4;

    int baseRadius = std::max(6, rockRadius_ + randomInt(-6, 6));

    // Base shadow/dark rock.
    float darkShade = randomFloat(0.45f, 0.65f);
    fillCircle(rgba, width_, height_, cx, cy, baseRadius,
               Color(baseColor_.r * darkShade, baseColor_.g * darkShade,
                     baseColor_.b * darkShade, baseColor_.a));

    // A few lighter overlapping circles for texture.
    int rockCount = std::max(2, rockCount_ + randomInt(-1, 2));
    for (int i = 0; i < rockCount; ++i) {
        int px = cx + randomInt(-baseRadius * 2 / 3, baseRadius * 2 / 3);
        int py = cy + randomInt(-baseRadius / 2, baseRadius / 3);
        int r = std::max(3, baseRadius * 2 / 3 + randomInt(-baseRadius / 3, baseRadius / 4));
        float shade = randomFloat(0.65f, 0.95f);
        fillCircle(rgba, width_, height_, px, py, r,
                   Color(baseColor_.r * shade, baseColor_.g * shade,
                         baseColor_.b * shade, baseColor_.a));
    }

    // Small highlight spot.
    fillCircle(rgba, width_, height_,
               cx + randomInt(-baseRadius / 3, baseRadius / 6),
               cy - baseRadius / 3,
               std::max(2, baseRadius / 4 + randomInt(-2, 2)),
               Color(std::min(1.0f, baseColor_.r * 1.15f),
                     std::min(1.0f, baseColor_.g * 1.15f),
                     std::min(1.0f, baseColor_.b * 1.15f),
                     baseColor_.a));

    return convert(width_, height_, format_, rgba);
}

} // namespace domi
