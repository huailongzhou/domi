#include "material_generator.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace domi {

namespace {

uint8_t toByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

uint16_t toRGB565(const Color& c) {
    uint8_t r = toByte(c.r);
    uint8_t g = toByte(c.g);
    uint8_t b = toByte(c.b);
    uint16_t rc = (r >> 3) & 0x1F;
    uint16_t gc = (g >> 2) & 0x3F;
    uint16_t bc = (b >> 3) & 0x1F;
    return (rc << 11) | (gc << 5) | bc;
}

} // anonymous namespace

void MaterialGeneratorBase::setPixel(std::vector<uint8_t>& buf, int w, int h,
                                   int x, int y, const Color& c) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    size_t idx = (y * w + x) * 4;
    buf[idx + 0] = toByte(c.a);
    buf[idx + 1] = toByte(c.r);
    buf[idx + 2] = toByte(c.g);
    buf[idx + 3] = toByte(c.b);
}

Color MaterialGeneratorBase::getPixel(const std::vector<uint8_t>& buf, int w, int h,
                                  int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return Color(0, 0, 0, 0);
    size_t idx = (y * w + x) * 4;
    return Color(buf[idx + 1] / 255.0f,
                 buf[idx + 2] / 255.0f,
                 buf[idx + 3] / 255.0f,
                 buf[idx + 0] / 255.0f);
}

void MaterialGeneratorBase::fillRect(std::vector<uint8_t>& buf, int w, int h,
                                 int x, int y, int rw, int rh, const Color& c) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(w, x + rw);
    int y1 = std::min(h, y + rh);
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            setPixel(buf, w, h, px, py, c);
        }
    }
}

void MaterialGeneratorBase::fillCircle(std::vector<uint8_t>& buf, int w, int h,
                                   int cx, int cy, int radius, const Color& c) {
    if (radius <= 0) return;
    int x0 = std::max(0, cx - radius);
    int y0 = std::max(0, cy - radius);
    int x1 = std::min(w, cx + radius + 1);
    int y1 = std::min(h, cy + radius + 1);
    int r2 = radius * radius;
    for (int py = y0; py < y1; ++py) {
        int dy = py - cy;
        for (int px = x0; px < x1; ++px) {
            int dx = px - cx;
            if (dx * dx + dy * dy <= r2) {
                setPixel(buf, w, h, px, py, c);
            }
        }
    }
}

void MaterialGeneratorBase::fillTriangle(std::vector<uint8_t>& buf, int w, int h,
                                         int x1, int y1, int x2, int y2,
                                         int x3, int y3, const Color& c) {
    int minX = std::min(std::min(x1, x2), x3);
    int minY = std::min(std::min(y1, y2), y3);
    int maxX = std::max(std::max(x1, x2), x3);
    int maxY = std::max(std::max(y1, y2), y3);

    minX = std::max(0, minX);
    minY = std::max(0, minY);
    maxX = std::min(w - 1, maxX);
    maxY = std::min(h - 1, maxY);

    // Barycentric point-in-triangle test.
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            bool a = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1) >= 0;
            bool b = (x3 - x2) * (py - y2) - (y3 - y2) * (px - x2) >= 0;
            bool c2 = (x1 - x3) * (py - y3) - (y1 - y3) * (px - x3) >= 0;
            if (a == b && b == c2) {
                setPixel(buf, w, h, px, py, c);
            }
        }
    }
}

Material MaterialGeneratorBase::convert(int width, int height, PixelFormat format,
                                    const std::vector<uint8_t>& rgba) {
    Material mat(width, height, format);
    const int count = width * height;

    switch (format) {
        case PixelFormat::ARGB8888:
            mat.pixels = rgba;
            break;

        case PixelFormat::RGB565: {
            mat.pixels.resize(count * 2);
            for (int i = 0; i < count; ++i) {
                Color c(rgba[i * 4 + 1] / 255.0f,
                        rgba[i * 4 + 2] / 255.0f,
                        rgba[i * 4 + 3] / 255.0f,
                        rgba[i * 4 + 0] / 255.0f);
                uint16_t v = toRGB565(c);
                mat.pixels[i * 2 + 0] = static_cast<uint8_t>(v & 0xFF);
                mat.pixels[i * 2 + 1] = static_cast<uint8_t>(v >> 8);
            }
            break;
        }

        case PixelFormat::AlphaMask: {
            mat.pixels.resize(count);
            for (int i = 0; i < count; ++i) {
                mat.pixels[i] = rgba[i * 4 + 0];
            }
            break;
        }

        case PixelFormat::LUT8: {
            // Build a palette from the actual colors used, capped at 256.
            std::vector<Color> palette;
            palette.reserve(256);
            std::vector<uint8_t> indices;
            indices.reserve(count);

            auto findOrAdd = [&palette](const Color& c) -> uint8_t {
                for (size_t i = 0; i < palette.size(); ++i) {
                    if (palette[i] == c) return static_cast<uint8_t>(i);
                }
                if (palette.size() < 256) {
                    palette.push_back(c);
                    return static_cast<uint8_t>(palette.size() - 1);
                }
                // Palette full: return nearest existing color.
                size_t best = 0;
                float bestDist = 1e10f;
                for (size_t i = 0; i < palette.size(); ++i) {
                    float dr = palette[i].r - c.r;
                    float dg = palette[i].g - c.g;
                    float db = palette[i].b - c.b;
                    float da = palette[i].a - c.a;
                    float d = dr * dr + dg * dg + db * db + da * da;
                    if (d < bestDist) {
                        bestDist = d;
                        best = i;
                    }
                }
                return static_cast<uint8_t>(best);
            };

            for (int i = 0; i < count; ++i) {
                Color c(rgba[i * 4 + 1] / 255.0f,
                        rgba[i * 4 + 2] / 255.0f,
                        rgba[i * 4 + 3] / 255.0f,
                        rgba[i * 4 + 0] / 255.0f);
                indices.push_back(findOrAdd(c));
            }

            mat.palette = std::move(palette);
            mat.pixels = std::move(indices);
            break;
        }
    }

    return mat;
}

} // namespace domi
