#ifndef DOMI_CAMERA2D_H
#define DOMI_CAMERA2D_H

namespace domi {

// A minimal 2D viewport camera.
//
// The camera maps world coordinates to screen coordinates as:
//   screen = world * zoom + offset
//
// World-space render passes (Geometry, Shadow, Lighting) apply this transform
// when the active scene exposes a Camera2D. Screen-space content
// (RenderLayer::Overlay and above, UI) is not affected.
struct Camera2D {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float zoom = 1.0f;
};

} // namespace domi

#endif // DOMI_CAMERA2D_H
