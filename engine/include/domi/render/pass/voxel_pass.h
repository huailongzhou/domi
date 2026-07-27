#ifndef DOMI_PASS_VOXEL_PASS_H
#define DOMI_PASS_VOXEL_PASS_H

#include "domi/render/render_pass.h"

namespace domi {

class VoxelRenderer;
class Canvas2D;

// Render all VoxelComponent chunks into the color buffer using the software
// voxel renderer. The pass manages the 3D block (begin3D/end3D) around the
// renderer so that multiple voxel chunks share one z-buffer composite.
class Voxel3DPass : public RenderPass {
public:
    Voxel3DPass();
    ~Voxel3DPass() override;

    void record(Canvas2D& canvas, RenderContext& ctx) override;

private:
    VoxelRenderer* renderer_;
};

} // namespace domi

#endif // DOMI_PASS_VOXEL_PASS_H
