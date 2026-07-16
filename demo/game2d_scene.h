#ifndef DEMO_GAME2D_SCENE_H
#define DEMO_GAME2D_SCENE_H

#include "domi/scene.h"
#include "domi/types.h"
#include "domi/math.h"
#include "domi/material.h"
#include "domi/camera2d.h"
#include <vector>

namespace domi {
class CustomNode;
class DrawBatch;
class MaterialNode;
class RectNode;
class RenderNode;
class ScriptSystem;
class World;
}

// The 2D demo scene: horizon, grass, lake, road, trees, house, rocks,
// clouds, a 3D car, a rotating 3D cube and a day/night cycle.
// Rendering is expressed as a RenderNode tree built once in load() and
// animated in update().
//
// The viewport can be panned by dragging with the left mouse button and
// zoomed with the mouse wheel (toward the cursor). Home resets it.
class Game2DScene : public domi::Scene {
public:
    static constexpr float kHorizonY = 240.0f;
    // The world is larger than the 1280x720 window; the camera pans/zooms
    // inside it and is clamped so the viewport never leaves the world.
    static constexpr float kWorldWidth = 2560.0f;
    static constexpr float kWorldHeight = 1440.0f;

    Game2DScene();

    const char* name() const override { return "Game2DScene"; }

    void load(domi::World* world, domi::ScriptSystem* script) override;
    void unload(domi::World* world, domi::ScriptSystem* script) override;
    void update(double dt) override;

    const domi::Camera2D* getCamera2D() const override { return &camera_; }

private:
    float carT_;
    float carSpeed_;
    float cloudOffset_;
    float cubeAngleX_;
    float cubeAngleY_;
    bool useZBuffer_;
    float dayTime_;      // 0..24 hours
    float dayDuration_;  // seconds for a full 24-hour cycle

    // Viewport camera controlled by the mouse.
    domi::Camera2D camera_;

    // Cached day-cycle state, computed once per frame in update().
    domi::Color cachedSunColor_;
    float cachedSunIntensity_;
    domi::Vec2 cachedLightDir_;
    domi::Color cachedOverlay_;

    domi::Entity sunEntity_;
    std::vector<domi::Entity> cloudEntities_;

    std::vector<domi::Material> treeTrunks_;
    std::vector<domi::Material> treeFoliages_;
    std::vector<domi::Material> cloudMaterials_;
    std::vector<domi::Material> rockMaterials_;
    domi::Material houseMaterial_;
    domi::Material horizonMaterial_;
    std::vector<int> horizonSkyline_; // texture-local hill-top y per x column

    // Layout data filled from the scene file (assets/scenes/game2d.json).
    struct CloudDef { float baseX; float y; float speed; };
    std::vector<CloudDef> clouds_;
    std::vector<domi::RenderNode*> treeNodes_; // TreeNode*, for ground shadows

    // Dynamic render nodes that are animated in update().
    std::vector<domi::MaterialNode*> cloudNodes_;
    domi::CustomNode* carNode_;
    domi::RectNode* overlayNode_;

    static domi::Color lerpColor(const domi::Color& a, const domi::Color& b, float t);
    void evaluateDayCycle(domi::Color& outSunColor, float& outIntensity,
                          domi::Vec2& outLightDir, domi::Color& outOverlay) const;
    void updateSun();
    void updateCamera();
    void clampCamera();
    void createShadowCasters(domi::World* world);
    void updateCloudShadowCasters();
    void buildRenderTree();
    void updateRenderNodes();
    domi::Vec2 cloudPosition(int i) const;
    float perspectiveScale(float y) const;

    void drawGroundShadows(domi::DrawBatch& batch);
    void draw3DCar(domi::DrawBatch& batch, float x, float y);
    void drawCube3D(domi::DrawBatch& batch, float cx, float cy, float size, float ax, float ay);
};

#endif // DEMO_GAME2D_SCENE_H
