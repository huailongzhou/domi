#ifndef DEMO_PHYSICS_SCENE_H
#define DEMO_PHYSICS_SCENE_H

#include "domi/scene/scene.h"
#include "domi/physics/physics.h"
#include "domi/core/types.h"
#include "domi/core/math.h"
#include <vector>

namespace domi {
class ScriptSystem;
class World;
class ClayUI;
class RectNode;
class EllipseNode;
}

// Box2D collision demo: platforms, bouncing crates, player, sensor pickup.
class PhysicsScene : public domi::Scene {
public:
    PhysicsScene();

    const char* name() const override { return "PhysicsScene"; }

    void load(domi::World* world, domi::ScriptSystem* script) override;
    void unload(domi::World* world, domi::ScriptSystem* script) override;
    void update(double dt) override;
    void fixedUpdate() override;

    bool buildClayUI(domi::ClayUI& ui) override;

private:
    struct BodyVisual {
        domi::Entity entity;
        b2Body* body;
        domi::RectNode* rect;       // box
        domi::EllipseNode* ellipse; // circle
        float w, h;
        bool isCircle;
        bool isSensor;
        bool collected;
        domi::Color baseColor;
        domi::Color flashColor;
        float flashTimer;
    };

    void spawnStatic(domi::World* world, float x, float y, float w, float h,
                     const domi::Color& color);
    void spawnBox(domi::World* world, float x, float y, float w, float h,
                  const domi::Color& color, float restitution = 0.2f);
    void spawnCircle(domi::World* world, float x, float y, float radius,
                     const domi::Color& color, float restitution = 0.6f);
    void spawnPickup(domi::World* world, float x, float y);
    void spawnPlayer(domi::World* world, float x, float y);
    void handleContacts();
    void syncVisuals();
    void rebuildSceneGraph();

    domi::PhysicsSystem physics_;
    domi::World* world_;
    std::vector<BodyVisual> visuals_;
    domi::Entity playerEntity_;
    b2Body* playerBody_;
    int score_;
    int contactFlashCount_;
    float moveSpeed_;
    float jumpImpulse_;
    bool grounded_;
    char statusBuf_[128];
};

#endif // DEMO_PHYSICS_SCENE_H
