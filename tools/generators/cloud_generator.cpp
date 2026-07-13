#include "cloud_generator.h"

#include <algorithm>

namespace domi {

Material CloudGenerator::build() {
    std::vector<uint8_t> rgba(width_ * height_ * 4, 0);

    int cx = width_ / 2;
    int cy = height_ / 2;

    // Randomize puff count and base radius, but cap the radius so that a
    // single puff can never exceed the material bounds.
    int puffCount = std::max(2, puffCount_ + randomInt(-2, 2));
    int maxAllowedRadius = std::min(width_, height_) / 2 - 4;
    if (maxAllowedRadius < 8) maxAllowedRadius = 8;
    int baseRadius = std::max(8, std::min(maxAllowedRadius,
                                          puffRadius_ + randomInt(-8, 8)));

    // Compute horizontal spread, keeping outer puffs inside the material.
    int usableWidth = width_ - 2 * baseRadius;
    if (usableWidth < baseRadius) usableWidth = baseRadius;
    int startX = cx - usableWidth / 2;
    int stepX = (puffCount > 1) ? usableWidth / (puffCount - 1) : 0;

    // Draw several overlapping puffs arranged horizontally.
    for (int i = 0; i < puffCount; ++i) {
        int r = std::max(4, std::min(maxAllowedRadius - 2,
                                     baseRadius + randomInt(-baseRadius / 3, baseRadius / 4)));

        // Constrain the center so the entire circle stays inside the material.
        int minX = r;
        int maxX = width_ - r - 1;
        int minY = r;
        int maxY = height_ - r - 1;
        if (minX > maxX) { minX = maxX = cx; }
        if (minY > maxY) { minY = maxY = cy; }

        int baseX = startX + i * stepX;
        int jitterX = randomInt(-baseRadius / 4, baseRadius / 4);
        int px = std::max(minX, std::min(maxX, baseX + jitterX));

        int jitterY = randomInt(-baseRadius / 4, baseRadius / 4);
        int py = std::max(minY, std::min(maxY, cy + jitterY));

        fillCircle(rgba, width_, height_, px, py, r, baseColor_);
    }

    return convert(width_, height_, format_, rgba);
}

} // namespace domi
