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
class PathNode;
}

// Box2D collision demo: platforms, bouncing crates, player, sensor pickup,
// a trampoline pad, and a spring-joint pendulum.
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
        domi::PathNode* path = NULL; // free-form quad (player, rotates)
        float w, h;
        bool isCircle;
        bool isSensor;
        bool collected;
        domi::Color baseColor;
        domi::Color flashColor;
        float flashTimer;
    };

    // A static pad that launches anything landing on it straight up.
    struct Trampoline {
        domi::Entity entity;
        b2Body* body;
        float x, y, w, h;          // plate center/size (pixels)
        float surfaceY;            // y the coil hangs down from (plate bottom)
        float baseY;               // y the coil stands on (floor/platform top)
        domi::PathNode* coil;      // zigzag under the plate
        float compressTimer;       // >0 while the launch animation plays
    };

    // A bob hanging from a fixed anchor on a spring (distance) joint.
    struct SpringPendulum {
        b2Body* anchorBody;
        b2Body* bobBody;
        domi::PathNode* coil;      // zigzag between anchor and bob
    };

    void spawnStatic(domi::World* world, float x, float y, float w, float h,
                     const domi::Color& color);
    void spawnBox(domi::World* world, float x, float y, float w, float h,
                  const domi::Color& color, float restitution = 0.2f,
                  float density = 1.0f);
    void spawnCircle(domi::World* world, float x, float y, float radius,
                     const domi::Color& color, float restitution = 0.6f,
                     float linearDamping = 0.2f);
    void spawnPickup(domi::World* world, float x, float y);
    void spawnPlayer(domi::World* world, float x, float y);
    void spawnTrampoline(domi::World* world, float x, float y, float baseY);
    void spawnSpringPendulum(domi::World* world, float ax, float ay,
                             float bx, float by, float radius);
    void handleContacts();
    void syncVisuals();
    void syncSpringVisuals();
    void rebuildSceneGraph();

    domi::PhysicsSystem physics_;
    domi::World* world_;
    std::vector<BodyVisual> visuals_;
    std::vector<Trampoline> trampolines_;
    std::vector<SpringPendulum> springs_;
    domi::Entity playerEntity_;
    b2Body* playerBody_;
    int score_;
    int contactFlashCount_;
    float moveSpeed_;
    float jumpImpulse_;
    float trampolineLaunchSpeed_;
    bool grounded_;
    int tipDir_;           // -1/1: losing balance toward this side, 0 = stable
    float tipAmount_;      // how far the COM hangs past the support edge (px)
    bool toppling_;        // currently tumbling off an edge
    bool charging_;        // holding DOWN on a trampoline
    float chargeTime_;     // seconds held, capped at 1
    int chargePadIndex_;   // trampolines_ index being charged, -1 = none
    char statusBuf_[128];
};

#endif // DEMO_PHYSICS_SCENE_H
