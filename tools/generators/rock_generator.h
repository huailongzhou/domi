#ifndef TOOLS_GENERATORS_ROCK_GENERATOR_H
#define TOOLS_GENERATORS_ROCK_GENERATOR_H

#include "material_generator.h"

namespace domi {

// Generates a gray rock/cluster of rocks material.
class RockGenerator : public MaterialGenerator<RockGenerator> {
public:
    RockGenerator& setRockCount(int count) {
        rockCount_ = count;
        return *this;
    }

    RockGenerator& setRockRadius(int radius) {
        rockRadius_ = radius;
        return *this;
    }

    Material build() override;

private:
    int rockCount_ = 3;
    int rockRadius_ = 18;
};

} // namespace domi

#endif // TOOLS_GENERATORS_ROCK_GENERATOR_H
