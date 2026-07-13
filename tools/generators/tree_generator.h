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

    // Generate only the trunk material.
    Material buildTrunk();

    // Generate only the foliage/canopy material.
    Material buildFoliage();

    // Generate a matched trunk + foliage pair in one call.
    void buildPair(Material& outTrunk, Material& outFoliage);

private:
    int trunkWidth_ = 12;
    int trunkHeight_ = 30;
    int foliageRadius_ = 28;
    Color highlightColor_ = Color(0.25f, 0.72f, 0.25f, 1.0f);

    // Shared randomization state for a matched trunk + foliage pair.
    struct TreeParams {
        int trunkW, trunkH;
        int trunkX, trunkY;
        int foliageRadius;
        int foliageY;
        int leafCount;
        Color trunkColor;
    };

    TreeParams generateParams();
    void rasterizeTrunk(std::vector<uint8_t>& rgba, const TreeParams& params);
    void rasterizeFoliage(std::vector<uint8_t>& rgba, const TreeParams& params);
};

} // namespace domi

#endif // TOOLS_GENERATORS_TREE_GENERATOR_H
