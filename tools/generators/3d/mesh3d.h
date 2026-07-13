#ifndef TOOLS_GENERATORS_3D_MESH3D_H
#define TOOLS_GENERATORS_3D_MESH3D_H

#include "domi/math.h"
#include <vector>

namespace domi {

// A simple indexed triangle mesh used by the Canvas2D software rasterizer.
struct Mesh3D {
    std::vector<Vec3> vertices;
    std::vector<int> indices;
    Color color = Color(1, 1, 1, 1);

    int triangleCount() const { return static_cast<int>(indices.size() / 3); }
};

} // namespace domi

#endif // TOOLS_GENERATORS_3D_MESH3D_H
