#ifndef DOMI_VOXEL_RENDERER_H
#define DOMI_VOXEL_RENDERER_H

#include "domi/math.h"
#include "domi/types.h"
#include <vector>

namespace domi {

class Canvas2D;
class World;
struct CameraComponent;
struct TransformComponent;

// Voxel renderer using the painter's algorithm.
//
// Each visible voxel face is projected to screen space and assigned a single
// depth value (the average of its four corner depths). All faces are sorted
// back-to-front and then submitted as 2D convex paths via Canvas2D. This avoids
// per-pixel CPU rasterization and depth testing: the backend (SDL_RenderGeometry
// or a future 2D hardware backend) handles the actual pixel fill.
class VoxelRenderer {
public:
    explicit VoxelRenderer(Canvas2D* canvas);

    // Render all voxel chunks in the world using the given camera.
    // If camera is NULL a default orthographic camera is used.
    void render(World* world, const CameraComponent* camera,
                const TransformComponent* cameraTransform,
                int screenW, int screenH);

private:
    struct Face {
        Vec2 screen[4];
        float z;
        Color color;
    };

    Canvas2D* canvas_;

    void renderChunk(World* world, Entity entity,
                     const Mat4& viewProj,
                     const Vec3& viewDir,
                     const TransformComponent* cameraTransform,
                     int screenW, int screenH,
                     std::vector<Face>& faces);
};

} // namespace domi

#endif // DOMI_VOXEL_RENDERER_H
