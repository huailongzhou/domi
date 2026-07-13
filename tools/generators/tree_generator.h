#ifndef TOOLS_GENERATORS_TREE_GENERATOR_H
#define TOOLS_GENERATORS_TREE_GENERATOR_H

#include "material_generator.h"

namespace domi {

// Generates a simple low-poly style tree material:
// a brown trunk plus several overlapping green foliage circles.
class TreeGenerator : public MaterialGenerator<TreeGenerator> {
public:
    TreeGenerator& setTrunkSize(int width, int height) {
        trunkWidth_ = width;
        trunkHeight_ = height;
        return *this;
    }

    TreeGenerator& setFoliageRadius(int radius) {
        foliageRadius_ = radius;
        return *this;
    }

    TreeGenerator& setHighlightColor(const Color& color) {
        highlightColor_ = color;
        return *this;
    }

    Material build() override;

private:
    int trunkWidth_ = 12;
    int trunkHeight_ = 30;
    int foliageRadius_ = 28;
    Color highlightColor_ = Color(0.25f, 0.72f, 0.25f, 1.0f);
};

} // namespace domi

#endif // TOOLS_GENERATORS_TREE_GENERATOR_H
