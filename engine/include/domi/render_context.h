#ifndef DOMI_RENDER_CONTEXT_H
#define DOMI_RENDER_CONTEXT_H

#include <cstddef>

namespace domi {

class RenderTexture;
class World;
struct CameraComponent;
struct LightComponent;

// Context passed to every RenderPass each frame.
// Holds the shared G-buffer textures and scene/light information.
struct RenderContext {
    RenderTexture* colorBuffer = NULL;
    RenderTexture* shadowMask = NULL;
    RenderTexture* lightBuffer = NULL;
    RenderTexture* finalBuffer = NULL;

    World* world = NULL;
    CameraComponent* camera = NULL;
    LightComponent* sun = NULL; // directional / global light

    int width = 0;
    int height = 0;
};

} // namespace domi

#endif // DOMI_RENDER_CONTEXT_H
