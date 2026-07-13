#include "tree3d_generator.h"

namespace domi {

Mesh3D Tree3DGenerator::makeBox(float cx, float cy, float cz,
                                float sx, float sy, float sz,
                                const Color& color) const {
    Mesh3D mesh;
    mesh.color = color;
    mesh.vertices = {
        Vec3(cx - 0.5f * sx, cy - 0.5f * sy, cz - 0.5f * sz),
        Vec3(cx + 0.5f * sx, cy - 0.5f * sy, cz - 0.5f * sz),
        Vec3(cx + 0.5f * sx, cy + 0.5f * sy, cz - 0.5f * sz),
        Vec3(cx - 0.5f * sx, cy + 0.5f * sy, cz - 0.5f * sz),
        Vec3(cx - 0.5f * sx, cy - 0.5f * sy, cz + 0.5f * sz),
        Vec3(cx + 0.5f * sx, cy - 0.5f * sy, cz + 0.5f * sz),
        Vec3(cx + 0.5f * sx, cy + 0.5f * sy, cz + 0.5f * sz),
        Vec3(cx - 0.5f * sx, cy + 0.5f * sy, cz + 0.5f * sz)
    };
    mesh.indices = {
        0, 3, 2, 0, 2, 1,
        5, 6, 7, 5, 7, 4,
        3, 7, 6, 3, 6, 2,
        4, 0, 1, 4, 1, 5,
        1, 2, 6, 1, 6, 5,
        4, 7, 3, 4, 3, 0
    };
    return mesh;
}

std::vector<Mesh3D> Tree3DGenerator::build() const {
    std::vector<Mesh3D> meshes;
    meshes.reserve(2);

    // Origin-centered trunk and foliage. The caller positions them on screen.
    meshes.push_back(makeBox(0.0f, 0.0f, 0.0f,
                             trunkW_, trunkH_, trunkD_, trunkColor_));

    meshes.push_back(makeBox(0.0f, 0.0f, 0.0f,
                             foliageW_, foliageH_, foliageD_, foliageColor_));

    return meshes;
}

} // namespace domi
