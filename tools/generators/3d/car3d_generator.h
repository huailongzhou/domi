#ifndef TOOLS_GENERATORS_3D_CAR3D_GENERATOR_H
#define TOOLS_GENERATORS_3D_CAR3D_GENERATOR_H

#include "mesh3d.h"
#include <vector>

namespace domi {

// Generates a simplified low-poly sedan mesh plus four wheel meshes.
class Car3DGenerator {
public:
    Car3DGenerator& setBodyColor(const Color& color) {
        bodyColor_ = color;
        return *this;
    }

    Car3DGenerator& setWheelColor(const Color& color) {
        wheelColor_ = color;
        return *this;
    }

    // Returns five meshes: body, front-left, front-right, back-left, back-right wheels.
    std::vector<Mesh3D> build() const;

private:
    Color bodyColor_ = Color(0.85f, 0.15f, 0.15f, 1.0f);
    Color wheelColor_ = Color(0.08f, 0.08f, 0.08f, 1.0f);

    Mesh3D makeBox(float cx, float cy, float cz,
                   float sx, float sy, float sz,
                   const Color& color) const;
};

} // namespace domi

#endif // TOOLS_GENERATORS_3D_CAR3D_GENERATOR_H
