#include "house_generator.h"

#include <algorithm>

namespace domi {

Material HouseGenerator::build() {
    std::vector<uint8_t> rgba(width_ * height_ * 4, 0);

    int cx = width_ / 2;

    // Randomize house proportions.
    int wallW = std::max(16, wallWidth_ + randomInt(-12, 12));
    int wallH = std::max(16, wallHeight_ + randomInt(-10, 10));
    int roofH = std::max(8, roofHeight_ + randomInt(-8, 8));

    // Position the house so it sits near the bottom of the material.
    int roofTopY = std::max(2, height_ - wallH - roofH);
    int roofBottomY = roofTopY + roofH;
    int wallBottomY = height_ - 2;

    int wallX = cx - wallW / 2;
    int wallY = roofBottomY;

    // Slightly vary wall color.
    Color wallColor(
        std::min(1.0f, baseColor_.r * randomFloat(0.9f, 1.1f)),
        std::min(1.0f, baseColor_.g * randomFloat(0.9f, 1.1f)),
        std::min(1.0f, baseColor_.b * randomFloat(0.9f, 1.1f)),
        baseColor_.a);

    // Walls.
    fillRect(rgba, width_, height_, wallX, wallY,
             wallW, wallBottomY - wallY, wallColor);

    // Roof (triangle) with randomized overhang.
    int overhang = randomInt(4, wallW / 4);
    int roofLeftX = wallX - overhang;
    int roofRightX = wallX + wallW + overhang;
    int roofPeakX = cx + randomInt(-wallW / 8, wallW / 8);
    fillTriangle(rgba, width_, height_,
                 roofLeftX, roofBottomY,
                 roofPeakX, roofTopY,
                 roofRightX, roofBottomY,
                 detailColor_);

    // Door.
    int doorW = std::max(4, wallW / 4 + randomInt(-3, 3));
    int doorH = (wallBottomY - wallY) * randomInt(50, 70) / 100;
    int doorX = cx - doorW / 2 + randomInt(-wallW / 8, wallW / 8);
    int doorY = wallBottomY - doorH;
    fillRect(rgba, width_, height_, doorX, doorY, doorW, doorH,
             Color(detailColor_.r * randomFloat(0.6f, 0.8f),
                   detailColor_.g * randomFloat(0.6f, 0.8f),
                   detailColor_.b * randomFloat(0.6f, 0.8f),
                   detailColor_.a));

    // Window.
    if (randomBool(0.8f)) {
        int winW = std::max(4, wallW / 4 + randomInt(-4, 4));
        int winH = std::max(4, wallW / 5 + randomInt(-3, 3));
        int winX = (randomBool() ? wallX + wallW / 6 : wallX + wallW * 5 / 6) - winW / 2;
        int winY = wallY + (wallBottomY - wallY) / 4 + randomInt(-5, 5);
        fillRect(rgba, width_, height_, winX, winY, winW, winH,
                 Color(0.75f, 0.85f, 0.95f, 1.0f));
    }

    return convert(width_, height_, format_, rgba);
}

} // namespace domi
