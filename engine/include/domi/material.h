#ifndef DOMI_MATERIAL_H
#define DOMI_MATERIAL_H

#include "domi/math.h"
#include <cstdint>
#include <string>
#include <vector>

namespace domi {

// Supported pixel formats for generated materials.
enum class PixelFormat {
    ARGB8888,   // 32-bit, 8 bits per channel
    RGB565,     // 16-bit, no alpha
    LUT8,       // 8-bit palette index + 256-color palette
    AlphaMask,  // 8-bit alpha only
};

// A generated 2D material: width x height pixels in the requested format.
struct Material {
    // Optional stable identifier (e.g. the name in a scene file). When set,
    // Canvas2D::drawMaterialCached keys its texture cache by this id instead
    // of the object's address, so the cache survives the Material being
    // moved, copied or regenerated in place.
    std::string id;

    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::ARGB8888;

    // Raw pixel data. Layout is row-major.
    // - ARGB8888:  4 bytes per pixel (A, R, G, B)
    // - RGB565:    2 bytes per pixel
    // - LUT8:      1 byte per pixel, index into palette
    // - AlphaMask: 1 byte per pixel, alpha value
    std::vector<uint8_t> pixels;

    // Only used when format == PixelFormat::LUT8.
    std::vector<Color> palette;

    Material() = default;
    Material(int w, int h, PixelFormat f)
        : width(w), height(h), format(f) {}
};

} // namespace domi

#endif // DOMI_MATERIAL_H
