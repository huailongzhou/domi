#ifndef DEMO_MENU_SCENE_H
#define DEMO_MENU_SCENE_H

#include "domi/scene.h"
#include "domi/ui/ui.h"

namespace domi {
class ScriptSystem;
class World;
}

// Entry menu with two buttons that switch to the 2D or 3D demo scenes.
class MenuScene : public domi::Scene {
public:
    MenuScene();

    const char* name() const override { return "MenuScene"; }

    domi::UIView* getUIRoot() override { return &menu_; }
    domi::UIContext* getUIContext() override { return &ui_; }

    void load(domi::World* world, domi::ScriptSystem* script) override;
    void unload(domi::World* world, domi::ScriptSystem* script) override;
    void update(double dt) override;

private:
    domi::UIContext ui_;
    domi::UIView menu_;
};

#endif // DEMO_MENU_SCENE_H
