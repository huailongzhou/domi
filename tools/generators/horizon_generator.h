#ifndef TOOLS_GENERATORS_HORIZON_GENERATOR_H
#define TOOLS_GENERATORS_HORIZON_GENERATOR_H

#include "material_generator.h"
#include <vector>

namespace domi {

// Generates a horizontal horizon strip that softens the transition between
// sky and ground. Produces layered hills and distant silhouettes.
class HorizonGenerator : public MaterialGenerator<HorizonGenerator> {
public:
    HorizonGenerator();

    HorizonGenerator& setSkyColor(const Color& color);
    HorizonGenerator& setHillColor(const Color& color);
    HorizonGenerator& setHillCount(int count);
    HorizonGenerator& setHillHeight(int minHeight, int maxHeight);

    Material build() override;

    // Return the sky/ground silhouette for each x column, in texture-local
    // coordinates (0 = top of the strip, height_-1 = bottom). A sprite whose
    // bottom is below this line is in front of the hills; a sprite whose bottom
    // is at or above this line is behind the horizon and should not cast
    // shadows on the ground.
    std::vector<int> buildSkyline();

private:
    Color skyColor_;
    Color hillColor_;
    int hillCount_;
    int hillMinHeight_;
    int hillMaxHeight_;
};

} // namespace domi

#endif // TOOLS_GENERATORS_HORIZON_GENERATOR_H
