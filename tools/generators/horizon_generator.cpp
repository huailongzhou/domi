#include "horizon_generator.h"
#include <cmath>
#include <algorithm>

namespace domi {

namespace {

float fract(float x) {
    return x - std::floor(x);
}

float hash(float n) {
    return fract(std::sin(n * 12.9898f) * 43758.5453f);
}

// Smooth 1D value noise in [0, 1].
float noise1D(float x) {
    float i = std::floor(x);
    float f = x - i;
    float u = f * f * (3.0f - 2.0f * f);
    return (1.0f - u) * hash(i) + u * hash(i + 1.0f);
}

} // anonymous namespace

HorizonGenerator::HorizonGenerator()
    : skyColor_(0.53f, 0.81f, 0.92f, 1.0f),
      hillColor_(0.18f, 0.45f, 0.18f, 1.0f),
      hillCount_(4),
      hillMinHeight_(20),
      hillMaxHeight_(60) {}

HorizonGenerator& HorizonGenerator::setSkyColor(const Color& color) {
    skyColor_ = color;
    return *this;
}

HorizonGenerator& HorizonGenerator::setHillColor(const Color& color) {
    hillColor_ = color;
    return *this;
}

HorizonGenerator& HorizonGenerator::setHillCount(int count) {
    hillCount_ = count;
    return *this;
}

HorizonGenerator& HorizonGenerator::setHillHeight(int minHeight, int maxHeight) {
    hillMinHeight_ = minHeight;
    hillMaxHeight_ = maxHeight;
    return *this;
}

Material HorizonGenerator::build() {
    std::vector<uint8_t> rgba(width_ * height_ * 4, 0);

    // Fill with transparent sky color.
    Color transparentSky(skyColor_.r, skyColor_.g, skyColor_.b, 0.0f);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            setPixel(rgba, width_, height_, x, y, transparentSky);
        }
    }

    // Vertical fade band in pixels for top and bottom edges.
    const int fadePixels = 16;

    // Generate layered hills from back to front.
    // Each hill layer is a sine + noise composite silhouette with alpha blending.
    for (int layer = 0; layer < hillCount_; ++layer) {
        float depth = (float)(layer + 1) / (float)hillCount_;
        int minH = hillMinHeight_ + static_cast<int>((hillMaxHeight_ - hillMinHeight_) * (1.0f - depth) * 0.5f);
        int maxH = hillMaxHeight_ - static_cast<int>((hillMaxHeight_ - hillMinHeight_) * depth * 0.3f);
        int baseHeight = minH + randomInt(0, maxH - minH);

        // Darken closer layers.
        Color layerColor(
            hillColor_.r * (0.6f + 0.4f * depth),
            hillColor_.g * (0.6f + 0.4f * depth),
            hillColor_.b * (0.6f + 0.4f * depth),
            1.0f);

        // Random phase and frequency for this layer.
        float phase1 = randomFloat(0.0f, 6.2831853f);
        float phase2 = randomFloat(0.0f, 6.2831853f);
        float freq1 = randomFloat(0.003f, 0.012f);
        float freq2 = randomFloat(0.01f, 0.03f);
        float amp1 = randomFloat(0.4f, 0.8f) * baseHeight;
        float amp2 = randomFloat(0.1f, 0.25f) * baseHeight;
        float noiseFreq = randomFloat(0.02f, 0.06f);
        float noiseAmp = randomFloat(0.15f, 0.3f) * baseHeight;

        for (int x = 0; x < width_; ++x) {
            float nx = static_cast<float>(x);
            float yf = static_cast<float>(baseHeight)
                + amp1 * std::sin(nx * freq1 + phase1)
                + amp2 * std::sin(nx * freq2 + phase2)
                + noiseAmp * noise1D(nx * noiseFreq);
            int hillTop = height_ - static_cast<int>(yf);
            if (hillTop < 0) hillTop = 0;
            if (hillTop >= height_) continue;

            // Fill from the hill top down to the bottom of the strip.
            for (int y = hillTop; y < height_; ++y) {
                // Alpha fades in from the top edge so the hills blend into the sky.
                // The bottom edge stays opaque so it sits flush against the grass.
                float alpha = 1.0f;
                int distTop = y - hillTop;
                if (distTop < fadePixels) {
                    alpha = static_cast<float>(distTop) / fadePixels;
                }
                if (alpha <= 0.0f) continue;

                Color src(layerColor.r, layerColor.g, layerColor.b, alpha);
                Color dst = getPixel(rgba, width_, height_, x, y);

                // Alpha-over blending.
                float outA = src.a + dst.a * (1.0f - src.a);
                if (outA <= 0.0f) continue;
                Color out(
                    (src.r * src.a + dst.r * dst.a * (1.0f - src.a)) / outA,
                    (src.g * src.a + dst.g * dst.a * (1.0f - src.a)) / outA,
                    (src.b * src.a + dst.b * dst.a * (1.0f - src.a)) / outA,
                    outA);
                setPixel(rgba, width_, height_, x, y, out);
            }
        }
    }

    // Blend the bottom edge into the grass base color so the horizon strip
    // sits flush against the scene's grass fillRect at y=240.
    int grassBlendHeight = height_ / 4;
    for (int y = height_ - grassBlendHeight; y < height_; ++y) {
        float t = static_cast<float>(y - (height_ - grassBlendHeight)) / grassBlendHeight;
        for (int x = 0; x < width_; ++x) {
            Color current = getPixel(rgba, width_, height_, x, y);
            Color blended(
                current.r * (1.0f - t) + baseColor_.r * t,
                current.g * (1.0f - t) + baseColor_.g * t,
                current.b * (1.0f - t) + baseColor_.b * t,
                current.a);
            setPixel(rgba, width_, height_, x, y, blended);
        }
    }

    return convert(width_, height_, format_, rgba);
}

} // namespace domi
