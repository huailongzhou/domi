#include "domi/physics/physics.h"

#include <box2d/box2d.h>
#include <cstring>
#include <algorithm>

namespace domi {

namespace {
const double kFixedDt = 1.0 / 60.0;
const int kVelocityIterations = 8;
const int kPositionIterations = 3;

Entity entityFromBody(const b2Body* body) {
    if (!body) return INVALID_ENTITY;
    return static_cast<Entity>(body->GetUserData().pointer);
}

ContactInfo makeContactInfo(b2Contact* contact, bool touching) {
    ContactInfo info;
    b2Fixture* fa = contact->GetFixtureA();
    b2Fixture* fb = contact->GetFixtureB();
    info.bodyA = fa->GetBody();
    info.bodyB = fb->GetBody();
    info.entityA = entityFromBody(info.bodyA);
    info.entityB = entityFromBody(info.bodyB);
    info.isTouching = touching;

    b2WorldManifold manifold;
    contact->GetWorldManifold(&manifold);
    info.normal = Vec2(manifold.normal.x, manifold.normal.y);
    info.point = Vec2(
        PhysicsSystem::toPixels(manifold.points[0].x),
        PhysicsSystem::toPixels(manifold.points[0].y));

    // Capture pre-solve velocities: after the step the solver has already
    // absorbed the impact, so impact speeds are only available here.
    b2Vec2 va = info.bodyA->GetLinearVelocity();
    b2Vec2 vb = info.bodyB->GetLinearVelocity();
    info.velA = Vec2(PhysicsSystem::toPixels(va.x), PhysicsSystem::toPixels(va.y));
    info.velB = Vec2(PhysicsSystem::toPixels(vb.x), PhysicsSystem::toPixels(vb.y));
    return info;
}
} // namespace

class PhysicsSystem::ContactListener : public b2ContactListener {
public:
    explicit ContactListener(PhysicsSystem* sys) : sys_(sys) {}

    void BeginContact(b2Contact* contact) override {
        ContactInfo info = makeContactInfo(contact, true);
        sys_->beginContacts_.push_back(info);
        sys_->activeContacts_.push_back(info);
    }

    void EndContact(b2Contact* contact) override {
        ContactInfo info = makeContactInfo(contact, false);
        sys_->endContacts_.push_back(info);

        std::vector<ContactInfo>& active = sys_->activeContacts_;
        for (size_t i = 0; i < active.size(); ++i) {
            const ContactInfo& c = active[i];
            bool same =
                (c.bodyA == info.bodyA && c.bodyB == info.bodyB) ||
                (c.bodyA == info.bodyB && c.bodyB == info.bodyA);
            if (same) {
                active[i] = active.back();
                active.pop_back();
                break;
            }
        }
    }

private:
    PhysicsSystem* sys_;
};

class PhysicsSystem::QueryCallback : public b2QueryCallback {
public:
    std::vector<Entity> entities;
    std::vector<b2Body*> bodies;

    bool ReportFixture(b2Fixture* fixture) override {
        b2Body* body = fixture->GetBody();
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i] == body) return true;
        }
        bodies.push_back(body);
        Entity e = entityFromBody(body);
        if (e != INVALID_ENTITY) entities.push_back(e);
        return true;
    }
};

class PhysicsSystem::RayCastCallback : public b2RayCastCallback {
public:
    RaycastHit hit;

    float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                        const b2Vec2& normal, float fraction) override {
        hit.hit = true;
        hit.body = fixture->GetBody();
        hit.entity = entityFromBody(hit.body);
        hit.point = Vec2(PhysicsSystem::toPixels(point.x),
                         PhysicsSystem::toPixels(point.y));
        hit.normal = Vec2(normal.x, normal.y);
        hit.fraction = fraction;
        return fraction;
    }
};

PhysicsSystem::PhysicsSystem()
    : world_(NULL), listener_(NULL), accumulator_(0.0) {}

PhysicsSystem::~PhysicsSystem() {
    shutdown();
}

bool PhysicsSystem::init(float gravityX, float gravityY) {
    shutdown();
    world_ = new b2World(b2Vec2(toMeters(gravityX), toMeters(gravityY)));
    world_->SetAllowSleeping(true);
    listener_ = new ContactListener(this);
    world_->SetContactListener(listener_);
    accumulator_ = 0.0;
    clearContacts();
    return true;
}

void PhysicsSystem::shutdown() {
    if (world_) {
        world_->SetContactListener(NULL);
    }
    delete listener_;
    listener_ = NULL;
    delete world_;
    world_ = NULL;
    clearContacts();
    accumulator_ = 0.0;
}

void PhysicsSystem::clearContacts() {
    beginContacts_.clear();
    endContacts_.clear();
    // activeContacts_ persists across steps until EndContact
}

void PhysicsSystem::step(double dt) {
    if (!world_) return;
    beginContacts_.clear();
    endContacts_.clear();

    accumulator_ += dt;
    if (accumulator_ > 0.25) accumulator_ = 0.25;
    while (accumulator_ >= kFixedDt) {
        world_->Step(static_cast<float>(kFixedDt),
                     kVelocityIterations, kPositionIterations);
        accumulator_ -= kFixedDt;
    }
}

b2BodyType PhysicsSystem::toB2(BodyType t) const {
    switch (t) {
        case BodyType::Static: return b2_staticBody;
        case BodyType::Kinematic: return b2_kinematicBody;
        case BodyType::Dynamic:
        default: return b2_dynamicBody;
    }
}

b2Body* PhysicsSystem::createBody(const BodyDef& def) {
    if (!world_) return NULL;

    b2BodyDef bd;
    bd.type = toB2(def.type);
    bd.position.Set(toMeters(def.x), toMeters(def.y));
    bd.fixedRotation = def.fixedRotation;
    bd.linearDamping = def.linearDamping;
    bd.angularDamping = def.angularDamping;
    bd.allowSleep = true;
    bd.userData.pointer = static_cast<uintptr_t>(def.entity);

    b2Body* body = world_->CreateBody(&bd);

    b2FixtureDef fd;
    fd.density = (def.type == BodyType::Dynamic) ? def.density : 0.0f;
    fd.friction = def.friction;
    fd.restitution = def.restitution;
    fd.isSensor = def.isSensor;
    fd.filter.categoryBits = def.categoryBits;
    fd.filter.maskBits = def.maskBits;

    if (def.shape == ShapeType::Circle) {
        b2CircleShape shape;
        float radius = def.width * 0.5f;
        if (def.height > 0.0f && def.shape == ShapeType::Circle) {
            // width is diameter when only width set; allow height as radius override via width
            radius = def.width * 0.5f;
        }
        shape.m_radius = toMeters(radius);
        fd.shape = &shape;
        body->CreateFixture(&fd);
    } else {
        b2PolygonShape shape;
        shape.SetAsBox(toMeters(def.width) * 0.5f, toMeters(def.height) * 0.5f);
        fd.shape = &shape;
        body->CreateFixture(&fd);
    }
    return body;
}

b2Body* PhysicsSystem::createBox(float x, float y, float w, float h, b2BodyType type,
                                 float density, float friction, float restitution) {
    BodyDef def;
    def.x = x;
    def.y = y;
    def.width = w;
    def.height = h;
    def.shape = ShapeType::Box;
    def.density = density;
    def.friction = friction;
    def.restitution = restitution;
    if (type == b2_staticBody) def.type = BodyType::Static;
    else if (type == b2_kinematicBody) def.type = BodyType::Kinematic;
    else def.type = BodyType::Dynamic;
    return createBody(def);
}

b2Body* PhysicsSystem::createCircle(float x, float y, float radius, b2BodyType type,
                                    float density, float friction, float restitution) {
    BodyDef def;
    def.x = x;
    def.y = y;
    def.width = radius * 2.0f;
    def.height = radius * 2.0f;
    def.shape = ShapeType::Circle;
    def.density = density;
    def.friction = friction;
    def.restitution = restitution;
    if (type == b2_staticBody) def.type = BodyType::Static;
    else if (type == b2_kinematicBody) def.type = BodyType::Kinematic;
    else def.type = BodyType::Dynamic;
    return createBody(def);
}

void PhysicsSystem::destroyBody(b2Body* body) {
    if (!world_ || !body) return;

    for (size_t i = 0; i < activeContacts_.size();) {
        if (activeContacts_[i].bodyA == body || activeContacts_[i].bodyB == body) {
            activeContacts_[i] = activeContacts_.back();
            activeContacts_.pop_back();
        } else {
            ++i;
        }
    }
    world_->DestroyBody(body);
}

void PhysicsSystem::setEntity(b2Body* body, Entity entity) {
    if (body) {
        body->GetUserData().pointer = static_cast<uintptr_t>(entity);
    }
}

Entity PhysicsSystem::getEntity(const b2Body* body) {
    return entityFromBody(body);
}

Vec2 PhysicsSystem::getPosition(const b2Body* body) {
    if (!body) return Vec2();
    b2Vec2 p = body->GetPosition();
    return Vec2(toPixels(p.x), toPixels(p.y));
}

float PhysicsSystem::getAngle(const b2Body* body) {
    return body ? body->GetAngle() : 0.0f;
}

Vec2 PhysicsSystem::getLinearVelocity(const b2Body* body) {
    if (!body) return Vec2();
    b2Vec2 v = body->GetLinearVelocity();
    return Vec2(toPixels(v.x), toPixels(v.y));
}

void PhysicsSystem::setPosition(b2Body* body, float x, float y) {
    if (!body) return;
    body->SetTransform(b2Vec2(toMeters(x), toMeters(y)), body->GetAngle());
}

void PhysicsSystem::setLinearVelocity(b2Body* body, float vx, float vy) {
    if (!body) return;
    body->SetLinearVelocity(b2Vec2(toMeters(vx), toMeters(vy)));
}

void PhysicsSystem::applyForce(b2Body* body, float fx, float fy) {
    if (!body) return;
    body->ApplyForceToCenter(b2Vec2(toMeters(fx), toMeters(fy)), true);
}

void PhysicsSystem::applyImpulse(b2Body* body, float ix, float iy) {
    if (!body) return;
    body->ApplyLinearImpulseToCenter(b2Vec2(toMeters(ix), toMeters(iy)), true);
}

b2Joint* PhysicsSystem::createSpringJoint(b2Body* bodyA, b2Body* bodyB,
                                          float ax, float ay, float bx, float by,
                                          float stiffness, float damping) {
    if (!world_ || !bodyA || !bodyB) return NULL;

    b2DistanceJointDef def;
    def.Initialize(bodyA, bodyB,
                   b2Vec2(toMeters(ax), toMeters(ay)),
                   b2Vec2(toMeters(bx), toMeters(by)));
    def.stiffness = stiffness;
    def.damping = damping;
    def.collideConnected = false;
    return world_->CreateJoint(&def);
}

bool PhysicsSystem::areTouching(Entity a, Entity b) const {
    if (a == INVALID_ENTITY || b == INVALID_ENTITY) return false;
    for (size_t i = 0; i < activeContacts_.size(); ++i) {
        const ContactInfo& c = activeContacts_[i];
        if ((c.entityA == a && c.entityB == b) ||
            (c.entityA == b && c.entityB == a)) {
            return true;
        }
    }
    return false;
}

bool PhysicsSystem::isTouching(Entity entity) const {
    if (entity == INVALID_ENTITY) return false;
    for (size_t i = 0; i < activeContacts_.size(); ++i) {
        if (activeContacts_[i].entityA == entity ||
            activeContacts_[i].entityB == entity) {
            return true;
        }
    }
    return false;
}

std::vector<Entity> PhysicsSystem::queryAABB(float x, float y, float w, float h) const {
    std::vector<Entity> empty;
    if (!world_) return empty;

    b2AABB aabb;
    aabb.lowerBound = b2Vec2(toMeters(x), toMeters(y));
    aabb.upperBound = b2Vec2(toMeters(x + w), toMeters(y + h));

    QueryCallback cb;
    world_->QueryAABB(&cb, aabb);
    return cb.entities;
}

RaycastHit PhysicsSystem::raycast(float x1, float y1, float x2, float y2) const {
    RaycastHit miss;
    if (!world_) return miss;

    RayCastCallback cb;
    world_->RayCast(&cb,
                    b2Vec2(toMeters(x1), toMeters(y1)),
                    b2Vec2(toMeters(x2), toMeters(y2)));
    return cb.hit;
}

} // namespace domi
