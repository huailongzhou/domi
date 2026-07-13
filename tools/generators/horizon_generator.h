#ifndef TOOLS_GENERATORS_HORIZON_GENERATOR_H
#define TOOLS_GENERATORS_HORIZON_GENERATOR_H

#include "material_generator.h"

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

private:
    Color skyColor_;
    Color hillColor_;
    int hillCount_;
    int hillMinHeight_;
    int hillMaxHeight_;
};

} // namespace domi

#endif // TOOLS_GENERATORS_HORIZON_GENERATOR_H
