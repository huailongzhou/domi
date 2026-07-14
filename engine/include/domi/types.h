#ifndef DOMI_TYPES_H
#define DOMI_TYPES_H

#include <cstdint>

namespace domi {

using Entity = uint32_t;
const Entity INVALID_ENTITY = 0;

// Texture blend modes for composite / blit operations.
// Mirrors SDL_BLENDMODE_* without exposing SDL headers in public interfaces.
enum class BlendMode {
    None,   // No blending (opaque overwrite).
    Blend,  // Alpha blending: src * alpha + dst * (1 - alpha).
    Add,    // Additive blending: src + dst.
    Mod     // Modulate blending: src * dst.
};

} // namespace domi

#endif
