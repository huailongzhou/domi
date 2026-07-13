#ifndef TOOLS_GENERATORS_3D_CUBE3D_GENERATOR_H
#define TOOLS_GENERATORS_3D_CUBE3D_GENERATOR_H

#include "mesh3d.h"
#include <vector>

namespace domi {

// Generates a low-poly colored cube as six separate face meshes.
// Each face can be drawn with its own color through Canvas2D::drawMesh3D.
class Cube3DGenerator {
public:
    Cube3DGenerator& setSize(float size) {
        size_ = size;
        return *this;
    }

    // Returns six meshes: front, back, top, bottom, right, left.
    std::vector<Mesh3D> build() const;

private:
    float size_ = 1.0f;
};

} // namespace domi

#endif // TOOLS_GENERATORS_3D_CUBE3D_GENERATOR_H
