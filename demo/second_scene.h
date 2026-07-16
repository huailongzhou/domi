#ifndef DEMO_SECOND_SCENE_H
#define DEMO_SECOND_SCENE_H

#include "domi/scene.h"
#include "domi/types.h"

namespace domi {
class ScriptSystem;
class World;
}

// Minimal scene that drives one sprite through the WASM script.
class SecondScene : public domi::Scene {
public:
    SecondScene();

    const char* name() const override { return "SecondScene"; }

    void load(domi::World* world, domi::ScriptSystem* script) override;
    void unload(domi::World* world, domi::ScriptSystem* script) override;
    void update(double dt) override;

private:
    domi::Entity blockEntity_;
};

#endif // DEMO_SECOND_SCENE_H
