#include "tree_generator.h"

#include <algorithm>

namespace domi {

Material TreeGenerator::build() {
    // Work in ARGB8888, then convert at the end.
    std::vector<uint8_t> rgba(width_ * height_ * 4, 0);

    // Center the tree in the material.
    int cx = width_ / 2;

    // Randomize trunk dimensions around the configured values.
    int trunkW = std::max(4, trunkWidth_ + randomInt(-3, 3));
    int trunkH = std::max(8, trunkHeight_ + randomInt(-6, 6));
    int trunkX = cx - trunkW / 2;
    int trunkY = height_ - trunkH;

    // Slightly vary the trunk color.
    Color trunkColor(
        std::min(1.0f, detailColor_.r * randomFloat(0.85f, 1.15f)),
        std::min(1.0f, detailColor_.g * randomFloat(0.85f, 1.15f)),
        std::min(1.0f, detailColor_.b * randomFloat(0.85f, 1.15f)),
        detailColor_.a);

    // Trunk.
    fillRect(rgba, width_, height_, trunkX, trunkY, trunkW, trunkH, trunkColor);

    // Randomize foliage cluster.
    int foliageRadius = std::max(8, foliageRadius_ + randomInt(-6, 6));
    int foliageY = trunkY - foliageRadius / 4 + randomInt(-5, 5);

    // Keep the foliage from dropping so low that it hides the entire trunk.
    // Allow it to cover roughly the upper half of the trunk at most.
    int minFoliageY = foliageRadius;
    int maxFoliageY = trunkY + trunkH / 2 - foliageRadius;
    if (maxFoliageY < minFoliageY) maxFoliageY = minFoliageY;
    foliageY = std::max(minFoliageY, std::min(maxFoliageY, foliageY));

    int leafCount = randomInt(3, 6);

    fillCircle(rgba, width_, height_, cx, foliageY, foliageRadius, baseColor_);

    for (int i = 0; i < leafCount; ++i) {
        int offsetX = randomInt(-foliageRadius * 3 / 4, foliageRadius * 3 / 4);
        int offsetY = randomInt(-foliageRadius / 2, foliageRadius / 2);
        int r = std::max(4, foliageRadius * 2 / 3 + randomInt(-6, 6));
        bool useHighlight = randomBool(0.6f);

        // Clamp child foliage centers so they also stay inside the material.
        int childX = std::max(r, std::min(width_ - r - 1, cx + offsetX));
        int childY = std::max(r, std::min(height_ - r - 1, foliageY + offsetY));

        fillCircle(rgba, width_, height_, childX, childY,
                   r, useHighlight ? highlightColor_ : baseColor_);
    }

    return convert(width_, height_, format_, rgba);
}

} // namespace domi
