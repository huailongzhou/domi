#ifndef DEMO_MENU_SCENE_H
#define DEMO_MENU_SCENE_H

#include "domi/scene.h"

namespace domi {
class ScriptSystem;
class World;
class ClayUI;
}

// Entry menu with two buttons that switch to the 2D or 3D demo scenes.
// The menu UI is declared with the Clay layout library (Scene::buildClayUI).
class MenuScene : public domi::Scene {
public:
    MenuScene();

    const char* name() const override { return "MenuScene"; }

    void load(domi::World* world, domi::ScriptSystem* script) override;
    void unload(domi::World* world, domi::ScriptSystem* script) override;
    void update(double dt) override;

    bool buildClayUI(domi::ClayUI& ui) override;

private:
    // Set by buildClayUI (render pass) and consumed by update() next frame —
    // scene switches must not happen mid-render.
    int pendingChoice_;
};

#endif // DEMO_MENU_SCENE_H
