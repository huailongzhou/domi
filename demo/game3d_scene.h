#ifndef DEMO_GAME3D_SCENE_H
#define DEMO_GAME3D_SCENE_H

#include "domi/scene.h"
#include "domi/types.h"
#include <vector>

namespace domi {
class ScriptSystem;
class World;
}

// The 3D demo scene: a small collect-the-cubes playground rendered
// through the ECS mesh pipeline.
class Game3DScene : public domi::Scene {
public:
    Game3DScene();

    const char* name() const override { return "Game3DScene"; }

    void load(domi::World* world, domi::ScriptSystem* script) override;
    void unload(domi::World* world, domi::ScriptSystem* script) override;
    void update(double dt) override;

private:
    domi::Entity player_;
    std::vector<domi::Entity> collectibles_;
    int score_;

    void spawnCollectibles(domi::World* world);
};

#endif // DEMO_GAME3D_SCENE_H
