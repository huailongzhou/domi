#include "cube3d_generator.h"

namespace domi {

std::vector<Mesh3D> Cube3DGenerator::build() const {
    const float s = size_;
    std::vector<Mesh3D> faces;
    faces.reserve(6);

    // Local cube vertices: centered at origin, edge length 1.
    std::vector<Vec3> verts = {
        Vec3(-0.5f * s, -0.5f * s, -0.5f * s), Vec3( 0.5f * s, -0.5f * s, -0.5f * s),
        Vec3( 0.5f * s,  0.5f * s, -0.5f * s), Vec3(-0.5f * s,  0.5f * s, -0.5f * s),
        Vec3(-0.5f * s, -0.5f * s,  0.5f * s), Vec3( 0.5f * s, -0.5f * s,  0.5f * s),
        Vec3( 0.5f * s,  0.5f * s,  0.5f * s), Vec3(-0.5f * s,  0.5f * s,  0.5f * s)
    };

    // Indices for each face (two triangles per face).
    struct FaceDef { int i0, i1, i2, i3; Color color; };
    FaceDef faceDefs[6] = {
        { 0, 3, 2, 1, Color(1.0f, 0.2f, 0.2f, 1.0f) }, // front
        { 5, 6, 7, 4, Color(0.2f, 1.0f, 0.2f, 1.0f) }, // back
        { 3, 7, 6, 2, Color(0.2f, 0.2f, 1.0f, 1.0f) }, // top
        { 4, 0, 1, 5, Color(1.0f, 1.0f, 0.2f, 1.0f) }, // bottom
        { 1, 2, 6, 5, Color(1.0f, 0.2f, 1.0f, 1.0f) }, // right
        { 4, 7, 3, 0, Color(0.2f, 1.0f, 1.0f, 1.0f) }  // left
    };

    for (int f = 0; f < 6; ++f) {
        const FaceDef& fd = faceDefs[f];
        Mesh3D mesh;
        mesh.vertices = verts;
        mesh.indices = {
            fd.i0, fd.i1, fd.i2,
            fd.i0, fd.i2, fd.i3
        };
        mesh.color = fd.color;
        faces.push_back(mesh);
    }

    return faces;
}

} // namespace domi
