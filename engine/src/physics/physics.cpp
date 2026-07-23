#include "domi/physics.h"

#include <box2d/box2d.h>

namespace domi {

namespace {
const double kFixedDt = 1.0 / 60.0;
const int kVelocityIterations = 8;
const int kPositionIterations = 3;
}

PhysicsSystem::PhysicsSystem() : world_(NULL), accumulator_(0.0) {}

PhysicsSystem::~PhysicsSystem() {
    shutdown();
}

bool PhysicsSystem::init(float gravityX, float gravityY) {
    shutdown();
    world_ = new b2World(b2Vec2(toMeters(gravityX), toMeters(gravityY)));
    accumulator_ = 0.0;
    return true;
}

void PhysicsSystem::shutdown() {
    delete world_;
    world_ = NULL;
}

void PhysicsSystem::step(double dt) {
    if (!world_) return;
    accumulator_ += dt;
    // Clamp the backlog so a long stall cannot spiral into hundreds of steps.
    if (accumulator_ > 0.25) accumulator_ = 0.25;
    while (accumulator_ >= kFixedDt) {
        world_->Step(static_cast<float>(kFixedDt),
                     kVelocityIterations, kPositionIterations);
        accumulator_ -= kFixedDt;
    }
}

b2Body* PhysicsSystem::createBox(float x, float y, float w, float h, b2BodyType type,
                                 float density, float friction, float restitution) {
    if (!world_) return NULL;
    b2BodyDef def;
    def.type = type;
    def.position.Set(toMeters(x), toMeters(y));
    b2Body* body = world_->CreateBody(&def);

    b2PolygonShape shape;
    shape.SetAsBox(toMeters(w) * 0.5f, toMeters(h) * 0.5f);
    if (type == b2_dynamicBody) {
        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.density = density;
        fixture.friction = friction;
        fixture.restitution = restitution;
        body->CreateFixture(&fixture);
    } else {
        body->CreateFixture(&shape, 0.0f);
    }
    return body;
}

b2Body* PhysicsSystem::createCircle(float x, float y, float radius, b2BodyType type,
                                    float density, float friction, float restitution) {
    if (!world_) return NULL;
    b2BodyDef def;
    def.type = type;
    def.position.Set(toMeters(x), toMeters(y));
    b2Body* body = world_->CreateBody(&def);

    b2CircleShape shape;
    shape.m_radius = toMeters(radius);
    if (type == b2_dynamicBody) {
        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.density = density;
        fixture.friction = friction;
        fixture.restitution = restitution;
        body->CreateFixture(&fixture);
    } else {
        body->CreateFixture(&shape, 0.0f);
    }
    return body;
}

void PhysicsSystem::destroyBody(b2Body* body) {
    if (world_ && body) world_->DestroyBody(body);
}

} // namespace domi
