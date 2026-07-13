#ifndef DOMI_GEOMETRY_PASS_H
#define DOMI_GEOMETRY_PASS_H

#include "domi/render_pass.h"

namespace domi {

class SceneManager;

// Renders the base scene color: ECS sprites plus the active Scene's Canvas2D
// content. Output goes to ctx.colorBuffer.
class GeometryPass : public RenderPass {
public:
    explicit GeometryPass(SceneManager* sceneManager = NULL);
    ~GeometryPass() override {}

    void setSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }
    SceneManager* getSceneManager() const { return sceneManager_; }

    void record(CommandBuffer& cmd, RenderContext& ctx) override;

private:
    SceneManager* sceneManager_;
};

} // namespace domi

#endif // DOMI_GEOMETRY_PASS_H
