#ifndef DOMI_PHYSICS_H
#define DOMI_PHYSICS_H

#include <box2d/box2d.h>

namespace domi {

// Thin wrapper over Box2D (third_party/box2d, v2.4.2).
//
// Conventions: positions and sizes are in PIXELS, y positive DOWN (matching
// the 2D renderer). Internally they are converted to meters at
// kPixelsPerMeter; gravity is given in pixels/s^2, so the default is
// (0, 490) ~= 9.8 m/s^2 downwards.
class PhysicsSystem {
public:
    static constexpr float kPixelsPerMeter = 50.0f;

    PhysicsSystem();
    ~PhysicsSystem();

    bool init(float gravityX = 0.0f, float gravityY = 490.0f);
    void shutdown();

    // Advances the simulation with a fixed 60 Hz timestep and an internal
    // accumulator (stable regardless of the render frame rate).
    void step(double dt);

    // Body factories (positions/sizes in pixels, y down). Box bodies are
    // centered on (x, y); dynamic bodies get the given density/friction/
    // restitution, static/kinematic ones ignore them.
    b2Body* createBox(float x, float y, float w, float h, b2BodyType type,
                      float density = 1.0f, float friction = 0.3f,
                      float restitution = 0.0f);
    b2Body* createCircle(float x, float y, float radius, b2BodyType type,
                         float density = 1.0f, float friction = 0.3f,
                         float restitution = 0.0f);
    void destroyBody(b2Body* body);

    b2World* world() { return world_; }

    // Pixel <-> meter conversion (for talking to the raw b2World).
    static float toMeters(float pixels) { return pixels / kPixelsPerMeter; }
    static float toPixels(float meters) { return meters * kPixelsPerMeter; }

private:
    b2World* world_;
    double accumulator_;
};

} // namespace domi

#endif // DOMI_PHYSICS_H
