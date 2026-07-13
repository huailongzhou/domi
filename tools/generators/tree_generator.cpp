#include "tree_generator.h"

#include <algorithm>

namespace domi {

TreeGenerator::TreeParams TreeGenerator::generateParams() {
    TreeParams p;
    int cx = width_ / 2;

    p.trunkW = std::max(4, trunkWidth_ + randomInt(-3, 3));
    p.trunkH = std::max(8, trunkHeight_ + randomInt(-6, 6));
    p.trunkX = cx - p.trunkW / 2;
    p.trunkY = height_ - p.trunkH;

    p.trunkColor = Color(
        std::min(1.0f, detailColor_.r * randomFloat(0.85f, 1.15f)),
        std::min(1.0f, detailColor_.g * randomFloat(0.85f, 1.15f)),
        std::min(1.0f, detailColor_.b * randomFloat(0.85f, 1.15f)),
        detailColor_.a);

    p.foliageRadius = std::max(8, foliageRadius_ + randomInt(-6, 6));
    int foliageY = p.trunkY - p.foliageRadius / 4 + randomInt(-5, 5);
    int minFoliageY = p.foliageRadius;
    int maxFoliageY = p.trunkY + p.trunkH / 2 - p.foliageRadius;
    if (maxFoliageY < minFoliageY) maxFoliageY = minFoliageY;
    p.foliageY = std::max(minFoliageY, std::min(maxFoliageY, foliageY));
    p.leafCount = randomInt(3, 6);

    return p;
}

void TreeGenerator::rasterizeTrunk(std::vector<uint8_t>& rgba, const TreeParams& params) {
    fillRect(rgba, width_, height_, params.trunkX, params.trunkY,
             params.trunkW, params.trunkH, params.trunkColor);
}

void TreeGenerator::rasterizeFoliage(std::vector<uint8_t>& rgba, const TreeParams& params) {
    int cx = width_ / 2;
    fillCircle(rgba, width_, height_, cx, params.foliageY,
               params.foliageRadius, baseColor_);
    for (int i = 0; i < params.leafCount; ++i) {
        int offsetX = randomInt(-params.foliageRadius * 3 / 4, params.foliageRadius * 3 / 4);
        int offsetY = randomInt(-params.foliageRadius / 2, params.foliageRadius / 2);
        int r = std::max(4, params.foliageRadius * 2 / 3 + randomInt(-6, 6));
        bool useHighlight = randomBool(0.6f);
        int childX = std::max(r, std::min(width_ - r - 1, cx + offsetX));
        int childY = std::max(r, std::min(height_ - r - 1, params.foliageY + offsetY));
        fillCircle(rgba, width_, height_, childX, childY,
                   r, useHighlight ? highlightColor_ : baseColor_);
    }
}

void TreeGenerator::buildPair(Material& outTrunk, Material& outFoliage) {
    TreeParams params = generateParams();

    std::vector<uint8_t> trunkRgba(width_ * height_ * 4, 0);
    rasterizeTrunk(trunkRgba, params);
    outTrunk = convert(width_, height_, format_, trunkRgba);

    std::vector<uint8_t> foliageRgba(width_ * height_ * 4, 0);
    rasterizeFoliage(foliageRgba, params);
    outFoliage = convert(width_, height_, format_, foliageRgba);
}

Material TreeGenerator::build() {
    Material trunk, foliage;
    buildPair(trunk, foliage);

    // Combine trunk and foliage into a single material.
    std::vector<uint8_t> rgba(width_ * height_ * 4, 0);
    for (int i = 0; i < width_ * height_; ++i) {
        size_t idx = i * 4;
        // Use foliage where it has alpha, otherwise trunk.
        uint8_t fa = foliage.pixels[idx + 0];
        if (fa > 0) {
            rgba[idx + 0] = fa;
            rgba[idx + 1] = foliage.pixels[idx + 1];
            rgba[idx + 2] = foliage.pixels[idx + 2];
            rgba[idx + 3] = foliage.pixels[idx + 3];
        } else {
            rgba[idx + 0] = trunk.pixels[idx + 0];
            rgba[idx + 1] = trunk.pixels[idx + 1];
            rgba[idx + 2] = trunk.pixels[idx + 2];
            rgba[idx + 3] = trunk.pixels[idx + 3];
        }
    }

    return convert(width_, height_, format_, rgba);
}

Material TreeGenerator::buildTrunk() {
    TreeParams params = generateParams();

    std::vector<uint8_t> rgba(width_ * height_ * 4, 0);
    rasterizeTrunk(rgba, params);

    return convert(width_, height_, format_, rgba);
}

Material TreeGenerator::buildFoliage() {
    TreeParams params = generateParams();

    std::vector<uint8_t> rgba(width_ * height_ * 4, 0);
    rasterizeFoliage(rgba, params);

    return convert(width_, height_, format_, rgba);
}

} // namespace domi
