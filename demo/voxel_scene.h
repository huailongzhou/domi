#ifndef DEMO_VOXEL_SCENE_H
#define DEMO_VOXEL_SCENE_H

#include "domi/scene/scene.h"

namespace domi {
class ScriptSystem;
class World;
}

// Demo scene that renders a voxel terrain through the Voxel3D renderer.
// Use the arrow keys to orbit the camera around the terrain.
class VoxelScene : public domi::Scene {
public:
    VoxelScene();

    const char* name() const override { return "VoxelScene"; }

    void load(domi::World* world, domi::ScriptSystem* script) override;
    void unload(domi::World* world, domi::ScriptSystem* script) override;
    void update(double dt) override;

private:
    domi::Entity camera_;
    domi::Entity terrain_;
    float cameraAngle_;
    float cameraHeight_;
};

#endif // DEMO_VOXEL_SCENE_H
