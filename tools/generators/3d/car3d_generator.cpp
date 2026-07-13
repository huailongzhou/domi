#include "car3d_generator.h"

namespace domi {

Mesh3D Car3DGenerator::makeBox(float cx, float cy, float cz,
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

std::vector<Mesh3D> Car3DGenerator::build() const {
    std::vector<Mesh3D> meshes;
    meshes.reserve(5);

    // Car body as a custom indexed mesh.
    {
        Mesh3D body;
        body.color = bodyColor_;
        body.vertices = {
            // 0-3: bottom chassis
            Vec3(-18.0f, -4.0f, -8.0f), Vec3( 18.0f, -4.0f, -8.0f),
            Vec3( 18.0f, -4.0f,  8.0f), Vec3(-18.0f, -4.0f,  8.0f),
            // 4-7: hood / lower body top
            Vec3(-18.0f,  4.0f, -8.0f), Vec3( 12.0f,  4.0f, -8.0f),
            Vec3( 12.0f,  4.0f,  8.0f), Vec3(-18.0f,  4.0f,  8.0f),
            // 8-11: cabin top
            Vec3(-6.0f,  14.0f, -7.0f), Vec3( 6.0f,  14.0f, -7.0f),
            Vec3( 6.0f,  14.0f,  7.0f), Vec3(-6.0f,  14.0f,  7.0f)
        };
        body.indices = {
            // bottom
            0, 2, 1, 0, 3, 2,
            // sides
            0, 1, 5, 0, 5, 4,
            1, 2, 6, 1, 6, 5,
            2, 3, 7, 2, 7, 6,
            3, 0, 4, 3, 4, 7,
            // hood
            4, 5, 6, 4, 6, 7,
            // cabin front
            5, 6, 10, 5, 10, 9,
            // cabin back
            7, 4, 8, 7, 8, 11,
            // cabin left
            4, 5, 9, 4, 9, 8,
            // cabin right
            6, 7, 11, 6, 11, 10,
            // cabin top
            8, 9, 10, 8, 10, 11
        };
        meshes.push_back(body);
    }

    // Four wheels as boxes.
    meshes.push_back(makeBox(-12.0f,  8.0f, 0.0f, 5.0f, 9.0f, 5.0f, wheelColor_));
    meshes.push_back(makeBox( 12.0f,  8.0f, 0.0f, 5.0f, 9.0f, 5.0f, wheelColor_));
    meshes.push_back(makeBox(-12.0f, -8.0f, 0.0f, 5.0f, 9.0f, 5.0f, wheelColor_));
    meshes.push_back(makeBox( 12.0f, -8.0f, 0.0f, 5.0f, 9.0f, 5.0f, wheelColor_));

    return meshes;
}

} // namespace domi
