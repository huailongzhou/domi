#ifndef TOOLS_GENERATORS_3D_TREE3D_GENERATOR_H
#define TOOLS_GENERATORS_3D_TREE3D_GENERATOR_H

#include "mesh3d.h"
#include <vector>

namespace domi {

// Generates a low-poly 3D tree as two boxes: a trunk and a foliage canopy.
class Tree3DGenerator {
public:
    Tree3DGenerator& setTrunkSize(float width, float height, float depth) {
        trunkW_ = width;
        trunkH_ = height;
        trunkD_ = depth;
        return *this;
    }

    Tree3DGenerator& setFoliageSize(float width, float height, float depth) {
        foliageW_ = width;
        foliageH_ = height;
        foliageD_ = depth;
        return *this;
    }

    Tree3DGenerator& setTrunkColor(const Color& color) {
        trunkColor_ = color;
        return *this;
    }

    Tree3DGenerator& setFoliageColor(const Color& color) {
        foliageColor_ = color;
        return *this;
    }

    // Returns two meshes: trunk and foliage.
    std::vector<Mesh3D> build() const;

private:
    float trunkW_ = 12.0f;
    float trunkH_ = 30.0f;
    float trunkD_ = 12.0f;

    float foliageW_ = 42.0f;
    float foliageH_ = 48.0f;
    float foliageD_ = 42.0f;

    Color trunkColor_ = Color(0.45f, 0.28f, 0.16f, 1.0f);
    Color foliageColor_ = Color(0.15f, 0.55f, 0.15f, 1.0f);

    Mesh3D makeBox(float cx, float cy, float cz,
                   float sx, float sy, float sz,
                   const Color& color) const;
};

} // namespace domi

#endif // TOOLS_GENERATORS_3D_TREE3D_GENERATOR_H
