#ifndef DOMI_PHYSICS_H
#define DOMI_PHYSICS_H

#include "domi/core/types.h"
#include "domi/core/math.h"
#include <box2d/box2d.h>
#include <vector>

namespace domi {

enum class BodyType {
    Static = 0,
    Kinematic = 1,
    Dynamic = 2
};

enum class ShapeType {
    Box = 0,
    Circle = 1
};

// Collision category bits (Box2D filter). Combine with | .
namespace CollisionCategory {
    const uint16_t Default  = 0x0001;
    const uint16_t Static   = 0x0002;
    const uint16_t Player   = 0x0004;
    const uint16_t Enemy    = 0x0008;
    const uint16_t Pickup   = 0x0010;
    const uint16_t Projectile = 0x0020;
    const uint16_t Sensor   = 0x0040;
    const uint16_t All      = 0xFFFF;
}

struct BodyDef {
    BodyType type;
    ShapeType shape;
    float x, y;           // center, pixels, y-down
    float width, height;  // box size (pixels); circle uses width as diameter
    float density;
    float friction;
    float restitution;
    float linearDamping;
    float angularDamping;
    bool isSensor;
    bool fixedRotation;
    uint16_t categoryBits;
    uint16_t maskBits;
    Entity entity;        // stored in b2Body userData

    BodyDef()
        : type(BodyType::Dynamic), shape(ShapeType::Box),
          x(0), y(0), width(32), height(32),
          density(1.0f), friction(0.3f), restitution(0.0f),
          linearDamping(0.1f), angularDamping(0.2f),
          isSensor(false), fixedRotation(false),
          categoryBits(CollisionCategory::Default),
          maskBits(CollisionCategory::All),
          entity(INVALID_ENTITY) {}
};

struct ContactInfo {
    Entity entityA;
    Entity entityB;
    b2Body* bodyA;
    b2Body* bodyB;
    bool isTouching;
    Vec2 normal;   // from A to B, pixel space (y-down)
    Vec2 point;    // contact point, pixels
};

struct RaycastHit {
    bool hit;
    Entity entity;
    b2Body* body;
    Vec2 point;
    Vec2 normal;
    float fraction;

    RaycastHit()
        : hit(false), entity(INVALID_ENTITY), body(NULL),
          point(), normal(), fraction(0.0f) {}
};

// Box2D wrapper with contact events, filters, and pixel-space helpers.
// Positions/sizes are PIXELS, y positive DOWN (matches 2D renderer).
// Internal unit: meters at kPixelsPerMeter. Gravity is pixels/s^2.
class PhysicsSystem {
public:
    static constexpr float kPixelsPerMeter = 50.0f;

    PhysicsSystem();
    ~PhysicsSystem();

    bool init(float gravityX = 0.0f, float gravityY = 490.0f);
    void shutdown();

    // Fixed 60 Hz step with internal accumulator.
    void step(double dt);

    b2Body* createBody(const BodyDef& def);
    b2Body* createBox(float x, float y, float w, float h, b2BodyType type,
                      float density = 1.0f, float friction = 0.3f,
                      float restitution = 0.0f);
    b2Body* createCircle(float x, float y, float radius, b2BodyType type,
                         float density = 1.0f, float friction = 0.3f,
                         float restitution = 0.0f);
    void destroyBody(b2Body* body);

    b2World* world() { return world_; }

    // Entity <-> body
    static void setEntity(b2Body* body, Entity entity);
    static Entity getEntity(const b2Body* body);

    // Pixel-space body state
    static Vec2 getPosition(const b2Body* body);
    static float getAngle(const b2Body* body); // radians, Box2D CCW
    static Vec2 getLinearVelocity(const b2Body* body);
    static void setPosition(b2Body* body, float x, float y);
    static void setLinearVelocity(b2Body* body, float vx, float vy);
    static void applyForce(b2Body* body, float fx, float fy);
    static void applyImpulse(b2Body* body, float ix, float iy);

    // Contacts from the last step (cleared at the start of each step).
    const std::vector<ContactInfo>& beginContacts() const { return beginContacts_; }
    const std::vector<ContactInfo>& endContacts() const { return endContacts_; }
    bool areTouching(Entity a, Entity b) const;
    bool isTouching(Entity entity) const;

    // Queries (pixels)
    std::vector<Entity> queryAABB(float x, float y, float w, float h) const;
    RaycastHit raycast(float x1, float y1, float x2, float y2) const;

    static float toMeters(float pixels) { return pixels / kPixelsPerMeter; }
    static float toPixels(float meters) { return meters * kPixelsPerMeter; }

private:
    class ContactListener;
    class QueryCallback;
    class RayCastCallback;

    b2BodyType toB2(BodyType t) const;
    void clearContacts();

    b2World* world_;
    ContactListener* listener_;
    double accumulator_;
    std::vector<ContactInfo> beginContacts_;
    std::vector<ContactInfo> endContacts_;
    std::vector<ContactInfo> activeContacts_;
};

} // namespace domi

#endif // DOMI_PHYSICS_H
