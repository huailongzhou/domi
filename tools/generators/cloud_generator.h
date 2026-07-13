#ifndef TOOLS_GENERATORS_CLOUD_GENERATOR_H
#define TOOLS_GENERATORS_CLOUD_GENERATOR_H

#include "material_generator.h"

namespace domi {

// Generates a fluffy cloud material from several overlapping circles.
class CloudGenerator : public MaterialGenerator<CloudGenerator> {
public:
    CloudGenerator& setPuffCount(int count) {
        puffCount_ = count;
        return *this;
    }

    CloudGenerator& setPuffRadius(int radius) {
        puffRadius_ = radius;
        return *this;
    }

    Material build() override;

private:
    int puffCount_ = 4;
    int puffRadius_ = 32;
};

} // namespace domi

#endif // TOOLS_GENERATORS_CLOUD_GENERATOR_H
