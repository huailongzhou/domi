#ifndef TOOLS_GENERATORS_HOUSE_GENERATOR_H
#define TOOLS_GENERATORS_HOUSE_GENERATOR_H

#include "material_generator.h"

namespace domi {

// Generates a simple house material: walls, a triangular roof, door and window.
class HouseGenerator : public MaterialGenerator<HouseGenerator> {
public:
    HouseGenerator& setWallSize(int width, int height) {
        wallWidth_ = width;
        wallHeight_ = height;
        return *this;
    }

    HouseGenerator& setRoofHeight(int height) {
        roofHeight_ = height;
        return *this;
    }

    Material build() override;

private:
    int wallWidth_ = 60;
    int wallHeight_ = 45;
    int roofHeight_ = 28;
};

} // namespace domi

#endif // TOOLS_GENERATORS_HOUSE_GENERATOR_H
