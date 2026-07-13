#ifndef TOOLS_GENERATORS_MATERIAL_GENERATOR_H
#define TOOLS_GENERATORS_MATERIAL_GENERATOR_H

#include "domi/material.h"
#include "domi/math.h"
#include <cstdint>
#include <vector>

namespace domi {

// Non-templated base holding only the rasterization helpers and the
// format-conversion logic. Subclasses normally inherit from MaterialGenerator.
class MaterialGeneratorBase {
protected:
    static void fillRect(std::vector<uint8_t>& buf, int w, int h,
                         int x, int y, int rw, int rh, const Color& c);

    static void fillCircle(std::vector<uint8_t>& buf, int w, int h,
                           int cx, int cy, int radius, const Color& c);

    static void fillTriangle(std::vector<uint8_t>& buf, int w, int h,
                             int x1, int y1, int x2, int y2,
                             int x3, int y3, const Color& c);

    static void setPixel(std::vector<uint8_t>& buf, int w, int h,
                         int x, int y, const Color& c);

    static Material convert(int width, int height, PixelFormat format,
                            const std::vector<uint8_t>& rgba);

    static Color getPixel(const std::vector<uint8_t>& buf, int w, int h,
                          int x, int y);
};

// Base class for material generators using the CRTP idiom so that the
// fluent interface returns the derived type and subclass-specific setters
// can be chained naturally.
//
// Example:
//   Material m = TreeGenerator()
//       .setSize(80, 80)
//       .setFormat(PixelFormat::ARGB8888)
//       .setBaseColor(Color(0.2f, 0.6f, 0.2f))
//       .setTrunkSize(14, 32)
//       .setFoliageRadius(30)
//       .build();
template <typename Derived>
class MaterialGenerator : public MaterialGeneratorBase {
public:
    virtual ~MaterialGenerator() {}

    // Common fluent setters. Return Derived& so subclass-specific setters
    // can be chained afterwards.
    Derived& setSize(int width, int height) {
        width_ = width;
        height_ = height;
        return derived();
    }

    Derived& setPosition(float x, float y) {
        posX_ = x;
        posY_ = y;
        return derived();
    }

    Derived& setFormat(PixelFormat format) {
        format_ = format;
        return derived();
    }

    Derived& setBaseColor(const Color& color) {
        baseColor_ = color;
        return derived();
    }

    Derived& setDetailColor(const Color& color) {
        detailColor_ = color;
        return derived();
    }

    // Seed the internal RNG. Without this the generator still produces
    // deterministic output (seed 0). Different seeds give different shapes.
    Derived& setSeed(unsigned seed) {
        rngState_ = seed;
        rngSeed_ = seed;
        rngSet_ = true;
        return derived();
    }

    // Rasterize and return the material in the configured format.
    virtual Material build() = 0;

protected:
    int width_ = 64;
    int height_ = 64;
    float posX_ = 0.0f;
    float posY_ = 0.0f;
    PixelFormat format_ = PixelFormat::ARGB8888;
    Color baseColor_ = Color(1.0f, 1.0f, 1.0f, 1.0f);
    Color detailColor_ = Color(0.0f, 0.0f, 0.0f, 1.0f);

    // Random number helpers. Use these in build() to vary shapes.
    float randomFloat(float min, float max) {
        return min + (max - min) * (nextRandom() / 4294967296.0f);
    }

    int randomInt(int min, int max) {
        if (min >= max) return min;
        return min + static_cast<int>(nextRandom() % static_cast<uint32_t>(max - min + 1));
    }

    bool randomBool(float probability = 0.5f) {
        return randomFloat(0.0f, 1.0f) < probability;
    }

    // Reset the RNG to the seed set by setSeed(), or to 0 if no seed was set.
    // Call this at the start of build() if the same generator instance may be
    // used to produce multiple related outputs (e.g. a material + a height map).
    void resetRng() {
        if (rngSet_) {
            rngState_ = rngSeed_;
        } else {
            rngState_ = 0;
        }
    }

private:
    unsigned rngState_ = 0;
    unsigned rngSeed_ = 0;
    bool rngSet_ = false;

    uint32_t nextRandom() {
        rngState_ = rngState_ * 1664525u + 1013904223u;
        return rngState_;
    }

    Derived& derived() { return static_cast<Derived&>(*this); }
};

} // namespace domi

#endif // TOOLS_GENERATORS_MATERIAL_GENERATOR_H
