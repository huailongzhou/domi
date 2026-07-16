#ifndef DOMI_RENDER_CONTEXT_H
#define DOMI_RENDER_CONTEXT_H

#include <cstddef>

namespace domi {

class RenderTexture;
class World;
class UIView;
class UIContext;
struct Camera2D;
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

    // Optional 2D viewport camera supplied by the active scene. World-space
    // passes apply it; screen-space layers (Overlay and above) ignore it.
    const Camera2D* camera2D = NULL;

    // Declarative UI overlay supplied by the active scene.
    UIView* uiRoot = NULL;
    UIContext* uiContext = NULL;

    // Frames per second, updated by the app each frame.
    float fps = 0.0f;

    int width = 0;
    int height = 0;
};

} // namespace domi

#endif // DOMI_RENDER_CONTEXT_H
