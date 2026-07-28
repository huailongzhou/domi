#include "physics_scene.h"

#include "domi/core/app.h"
#include "domi/ecs/ecs.h"
#include "domi/ecs/component.h"
#include "domi/input/input.h"
#include "domi/scene/scene_manager.h"
#include "domi/render/render_node.h"
#include "domi/ui/clay_ui.h"
#include "menu_scene.h"
#include <cstdio>
#include <cmath>
#include <cstring>

using namespace domi;

namespace {
const float kWorldW = 1280.0f;
const float kWorldH = 720.0f;
}

PhysicsScene::PhysicsScene()
    : world_(NULL), playerEntity_(INVALID_ENTITY), playerBody_(NULL),
      score_(0), contactFlashCount_(0), moveSpeed_(320.0f),
      jumpImpulse_(520.0f), trampolineLaunchSpeed_(850.0f), grounded_(false),
      tipDir_(0), tipAmount_(0.0f), toppling_(false),
      charging_(false), chargeTime_(0.0f), chargePadIndex_(-1) {
    statusBuf_[0] = '\0';
    std::snprintf(statusBuf_, sizeof(statusBuf_), "WASD/Arrows move, Space jump, R menu");
}

void PhysicsScene::spawnStatic(World* world, float x, float y, float w, float h,
                               const Color& color) {
    Entity e = world->createEntity();
    TransformComponent* t = world->addComponent<TransformComponent>(e);
    t->transform.position = Vec3(x, y, 0);
    t->transform.scale = Vec3(1, 1, 1);

    BodyDef def;
    def.type = BodyType::Static;
    def.shape = ShapeType::Box;
    def.x = x;
    def.y = y;
    def.width = w;
    def.height = h;
    def.friction = 0.6f;
    def.categoryBits = CollisionCategory::Static;
    def.entity = e;
    b2Body* body = physics_.createBody(def);

    RigidBodyComponent* rb = world->addComponent<RigidBodyComponent>(e);
    rb->body = body;
    rb->shape = 0;
    rb->width = w;
    rb->height = h;
    rb->categoryBits = def.categoryBits;
    rb->maskBits = def.maskBits;

    BodyVisual v;
    v.entity = e;
    v.body = body;
    v.rect = NULL;
    v.ellipse = NULL;
    v.w = w;
    v.h = h;
    v.isCircle = false;
    v.isSensor = false;
    v.collected = false;
    v.baseColor = color;
    v.flashColor = color;
    v.flashTimer = 0.0f;
    visuals_.push_back(v);
}

void PhysicsScene::spawnBox(World* world, float x, float y, float w, float h,
                            const Color& color, float restitution, float density) {
    Entity e = world->createEntity();
    TransformComponent* t = world->addComponent<TransformComponent>(e);
    t->transform.position = Vec3(x, y, 0);
    t->transform.scale = Vec3(1, 1, 1);

    BodyDef def;
    def.type = BodyType::Dynamic;
    def.shape = ShapeType::Box;
    def.x = x;
    def.y = y;
    def.width = w;
    def.height = h;
    def.density = density;
    def.friction = 0.4f;
    def.restitution = restitution;
    def.categoryBits = CollisionCategory::Default;
    def.entity = e;
    b2Body* body = physics_.createBody(def);

    RigidBodyComponent* rb = world->addComponent<RigidBodyComponent>(e);
    rb->body = body;
    rb->shape = 0;
    rb->width = w;
    rb->height = h;
    rb->categoryBits = def.categoryBits;
    rb->maskBits = def.maskBits;

    BodyVisual v;
    v.entity = e;
    v.body = body;
    v.rect = NULL;
    v.ellipse = NULL;
    v.w = w;
    v.h = h;
    v.isCircle = false;
    v.isSensor = false;
    v.collected = false;
    v.baseColor = color;
    v.flashColor = Color(1, 1, 0.4f, 1);
    v.flashTimer = 0.0f;
    visuals_.push_back(v);
}

void PhysicsScene::spawnCircle(World* world, float x, float y, float radius,
                               const Color& color, float restitution,
                               float linearDamping) {
    Entity e = world->createEntity();
    TransformComponent* t = world->addComponent<TransformComponent>(e);
    t->transform.position = Vec3(x, y, 0);
    t->transform.scale = Vec3(1, 1, 1);

    BodyDef def;
    def.type = BodyType::Dynamic;
    def.shape = ShapeType::Circle;
    def.x = x;
    def.y = y;
    def.width = radius * 2.0f;
    def.height = radius * 2.0f;
    def.density = 0.8f;
    def.friction = 0.5f;
    def.restitution = restitution;
    def.linearDamping = linearDamping;
    def.angularDamping = 0.4f;
    def.categoryBits = CollisionCategory::Default;
    def.entity = e;
    b2Body* body = physics_.createBody(def);

    RigidBodyComponent* rb = world->addComponent<RigidBodyComponent>(e);
    rb->body = body;
    rb->shape = 1;
    rb->width = radius * 2.0f;
    rb->height = radius * 2.0f;
    rb->categoryBits = def.categoryBits;
    rb->maskBits = def.maskBits;

    BodyVisual v;
    v.entity = e;
    v.body = body;
    v.rect = NULL;
    v.ellipse = NULL;
    v.w = radius * 2.0f;
    v.h = radius * 2.0f;
    v.isCircle = true;
    v.isSensor = false;
    v.collected = false;
    v.baseColor = color;
    v.flashColor = Color(1, 1, 0.4f, 1);
    v.flashTimer = 0.0f;
    visuals_.push_back(v);
}

void PhysicsScene::spawnPickup(World* world, float x, float y) {
    const float size = 28.0f;
    Entity e = world->createEntity();
    TransformComponent* t = world->addComponent<TransformComponent>(e);
    t->transform.position = Vec3(x, y, 0);
    t->transform.scale = Vec3(1, 1, 1);

    BodyDef def;
    def.type = BodyType::Static;
    def.shape = ShapeType::Circle;
    def.x = x;
    def.y = y;
    def.width = size;
    def.height = size;
    def.isSensor = true;
    def.categoryBits = CollisionCategory::Pickup;
    def.maskBits = CollisionCategory::Player;
    def.entity = e;
    b2Body* body = physics_.createBody(def);

    RigidBodyComponent* rb = world->addComponent<RigidBodyComponent>(e);
    rb->body = body;
    rb->shape = 1;
    rb->width = size;
    rb->height = size;
    rb->isSensor = true;
    rb->categoryBits = def.categoryBits;
    rb->maskBits = def.maskBits;

    BodyVisual v;
    v.entity = e;
    v.body = body;
    v.rect = NULL;
    v.ellipse = NULL;
    v.w = size;
    v.h = size;
    v.isCircle = true;
    v.isSensor = true;
    v.collected = false;
    v.baseColor = Color(1.0f, 0.85f, 0.2f, 1.0f);
    v.flashColor = Color(1, 1, 1, 1);
    v.flashTimer = 0.0f;
    visuals_.push_back(v);
}

namespace {

// Record a spring-coil zigzag from (x1, y1) to (x2, y2) into the path node.
// The amplitude tapers to zero at both ends so the coil meets its anchors.
void buildCoil(domi::PathNode& node, float x1, float y1, float x2, float y2,
               int coils, float amp) {
    node.clear();
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    node.moveTo(x1, y1);
    if (len > 1.0f) {
        float px = -dy / len;
        float py = dx / len;
        int segs = coils * 2;
        for (int i = 1; i < segs; ++i) {
            float t = (float)i / (float)segs;
            float taper = std::sin(3.14159265f * t);
            float off = ((i % 2) ? amp : -amp) * taper;
            node.lineTo(x1 + dx * t + px * off, y1 + dy * t + py * off);
        }
    }
    node.lineTo(x2, y2);
}

// Record a w x h quad centered at (cx, cy), rotated by angle (radians) and
// shifted down by yOff, into the path node. Used for the player sprite.
void buildRotatedQuad(domi::PathNode& node, float cx, float cy,
                      float w, float h, float angle, float yOff) {
    float ca = std::cos(angle);
    float sa = std::sin(angle);
    float hw = w * 0.5f;
    float hh = h * 0.5f;
    const float lx[4] = { -hw, hw, hw, -hw };
    const float ly[4] = { -hh, -hh, hh, hh };
    node.clear();
    for (int k = 0; k < 4; ++k) {
        float qx = cx + lx[k] * ca - ly[k] * sa;
        float qy = cy + lx[k] * sa + ly[k] * ca + yOff;
        if (k == 0) node.moveTo(qx, qy);
        else node.lineTo(qx, qy);
    }
    node.closePath();
}

} // namespace

void PhysicsScene::spawnTrampoline(World* world, float x, float y, float baseY) {
    const float w = 70.0f;
    const float h = 10.0f;

    Entity e = world->createEntity();
    TransformComponent* t = world->addComponent<TransformComponent>(e);
    t->transform.position = Vec3(x, y, 0);
    t->transform.scale = Vec3(1, 1, 1);

    BodyDef def;
    def.type = BodyType::Static;
    def.shape = ShapeType::Box;
    def.x = x;
    def.y = y;
    def.width = w;
    def.height = h;
    def.friction = 0.6f;
    def.categoryBits = CollisionCategory::Static;
    def.entity = e;
    b2Body* body = physics_.createBody(def);

    RigidBodyComponent* rb = world->addComponent<RigidBodyComponent>(e);
    rb->body = body;
    rb->shape = 0;
    rb->width = w;
    rb->height = h;
    rb->categoryBits = def.categoryBits;
    rb->maskBits = def.maskBits;

    BodyVisual v;
    v.entity = e;
    v.body = body;
    v.rect = NULL;
    v.ellipse = NULL;
    v.w = w;
    v.h = h;
    v.isCircle = false;
    v.isSensor = false;
    v.collected = false;
    v.baseColor = Color(0.95f, 0.75f, 0.20f, 1.0f);
    v.flashColor = Color(1.0f, 0.90f, 0.40f, 1.0f);
    v.flashTimer = 0.0f;
    visuals_.push_back(v);

    Trampoline tr;
    tr.entity = e;
    tr.body = body;
    tr.x = x;
    tr.y = y;
    tr.w = w;
    tr.h = h;
    tr.surfaceY = y + h * 0.5f;
    tr.baseY = baseY;
    tr.coil = NULL;
    tr.compressTimer = 0.0f;
    trampolines_.push_back(tr);
}

void PhysicsScene::spawnSpringPendulum(World* world, float ax, float ay,
                                       float bx, float by, float radius) {
    // Fixed anchor the spring hangs from.
    spawnStatic(world, ax, ay, 18.0f, 18.0f, Color(0.50f, 0.55f, 0.65f));
    b2Body* anchor = visuals_.back().body;

    // The bob, spawned off to the side so it swings as well as bounces.
    spawnCircle(world, bx, by, radius, Color(0.40f, 0.85f, 0.90f), 0.3f);
    b2Body* bob = visuals_.back().body;

    // Spring (distance) joint; rest length is the initial anchor distance.
    physics_.createSpringJoint(anchor, bob, ax, ay, bx, by, 25.0f, 0.6f);

    SpringPendulum s;
    s.anchorBody = anchor;
    s.bobBody = bob;
    s.coil = NULL;
    springs_.push_back(s);
}

void PhysicsScene::spawnPlayer(World* world, float x, float y) {
    const float w = 36.0f;
    const float h = 48.0f;
    playerEntity_ = world->createEntity();
    TransformComponent* t = world->addComponent<TransformComponent>(playerEntity_);
    t->transform.position = Vec3(x, y, 0);
    t->transform.scale = Vec3(1, 1, 1);

    BodyDef def;
    def.type = BodyType::Dynamic;
    def.shape = ShapeType::Box;
    def.x = x;
    def.y = y;
    def.width = w;
    def.height = h;
    def.density = 1.2f;
    def.friction = 0.0f;
    def.restitution = 0.0f;
    def.fixedRotation = true;
    def.categoryBits = CollisionCategory::Player;
    def.maskBits = static_cast<uint16_t>(
        CollisionCategory::Static | CollisionCategory::Default |
        CollisionCategory::Pickup | CollisionCategory::Enemy);
    def.entity = playerEntity_;
    playerBody_ = physics_.createBody(def);

    // Slightly lower friction on feet via extra fixture not needed for demo.
    playerBody_->SetSleepingAllowed(false);

    RigidBodyComponent* rb = world->addComponent<RigidBodyComponent>(playerEntity_);
    rb->body = playerBody_;
    rb->shape = 0;
    rb->width = w;
    rb->height = h;
    rb->categoryBits = def.categoryBits;
    rb->maskBits = def.maskBits;

    BodyVisual v;
    v.entity = playerEntity_;
    v.body = playerBody_;
    v.rect = NULL;
    v.ellipse = NULL;
    v.w = w;
    v.h = h;
    v.isCircle = false;
    v.isSensor = false;
    v.collected = false;
    v.baseColor = Color(0.25f, 0.75f, 1.0f, 1.0f);
    v.flashColor = Color(1.0f, 0.5f, 0.2f, 1.0f);
    v.flashTimer = 0.0f;
    visuals_.push_back(v);
}

void PhysicsScene::rebuildSceneGraph() {
    std::unique_ptr<GroupNode> root(new GroupNode());
    LayerView& bg = root->backgroundLayer();
    bg.addChild<RectNode>(0, 0, kWorldW, kWorldH, Color(0.10f, 0.12f, 0.16f)).sortByTop();

    // Decorative sky strip
    bg.addChild<RectNode>(0, 0, kWorldW, 180.0f, Color(0.14f, 0.18f, 0.28f)).sortByTop();

    LayerView& ground = root->groundLayer();
    LayerView& objects = root->objectLayer();

    for (size_t i = 0; i < visuals_.size(); ++i) {
        BodyVisual& v = visuals_[i];
        if (v.collected || !v.body) continue;

        Vec2 p = PhysicsSystem::getPosition(v.body);
        Color c = v.flashTimer > 0.0f ? v.flashColor : v.baseColor;
        if (v.isSensor) {
            c = Color(c.r, c.g, c.b, 0.9f);
        }

        if (v.isCircle) {
            float r = v.w * 0.5f;
            EllipseNode& el = objects.addChild<EllipseNode>(p.x, p.y, r, r);
            el.setFillColor(c).setStrokeColor(Color(0, 0, 0, 0.35f)).setLineWidth(2.0f);
            el.sortByCenterY();
            v.ellipse = &el;
            v.rect = NULL;
        } else if (v.entity == playerEntity_) {
            // The player is a free-form quad so it can rotate when toppling.
            PathNode& pn = objects.addChild<PathNode>();
            pn.setFillColor(c).setStrokeColor(Color(0, 0, 0, 0.35f)).setLineWidth(2.0f);
            pn.sortByCenterY();
            v.path = &pn;
            v.rect = NULL;
            v.ellipse = NULL;
        } else {
            float left = p.x - v.w * 0.5f;
            float top = p.y - v.h * 0.5f;
            // Static platforms go on ground layer; dynamics on object layer.
            LayerView& layer = (v.body->GetType() == b2_staticBody && !v.isSensor)
                                   ? ground
                                   : objects;
            RectNode& rect = layer.addChild<RectNode>(left, top, v.w, v.h, c);
            rect.sortByBottom();
            v.rect = &rect;
            v.ellipse = NULL;
        }
    }

    // Trampoline coils under the plates.
    for (size_t i = 0; i < trampolines_.size(); ++i) {
        Trampoline& tr = trampolines_[i];
        PathNode& coil = ground.addChild<PathNode>();
        coil.setStrokeColor(Color(0.85f, 0.85f, 0.90f, 1.0f)).setLineWidth(2.5f);
        coil.sortByBottom();
        tr.coil = &coil;
    }

    // Spring pendulum coils between anchor and bob.
    for (size_t i = 0; i < springs_.size(); ++i) {
        SpringPendulum& s = springs_[i];
        PathNode& coil = objects.addChild<PathNode>();
        coil.setStrokeColor(Color(0.90f, 0.90f, 0.95f, 1.0f)).setLineWidth(2.0f);
        coil.sortByCenterY();
        s.coil = &coil;
    }

    setRootNode(std::move(root));
}

void PhysicsScene::load(World* world, ScriptSystem* script) {
    (void)script;
    world_ = world;
    visuals_.clear();
    trampolines_.clear();
    springs_.clear();
    score_ = 0;
    contactFlashCount_ = 0;
    grounded_ = false;
    playerEntity_ = INVALID_ENTITY;
    playerBody_ = NULL;

    physics_.init(0.0f, 980.0f);

    // Floor and walls
    spawnStatic(world, kWorldW * 0.5f, kWorldH - 20.0f, kWorldW, 40.0f,
                Color(0.28f, 0.32f, 0.38f));
    spawnStatic(world, 20.0f, kWorldH * 0.5f, 40.0f, kWorldH,
                Color(0.22f, 0.25f, 0.30f));
    spawnStatic(world, kWorldW - 20.0f, kWorldH * 0.5f, 40.0f, kWorldH,
                Color(0.22f, 0.25f, 0.30f));

    // Platforms
    spawnStatic(world, 320.0f, 520.0f, 220.0f, 24.0f, Color(0.35f, 0.45f, 0.55f));
    spawnStatic(world, 700.0f, 420.0f, 200.0f, 24.0f, Color(0.35f, 0.45f, 0.55f));
    spawnStatic(world, 980.0f, 540.0f, 180.0f, 24.0f, Color(0.35f, 0.45f, 0.55f));

    // Force-transmission rig: a DYNAMIC wooden slab resting on two static
    // pillars that float above the floor (the floor path underneath stays
    // walkable). A static body would absorb the headbutt impulse entirely
    // (infinite mass); a dynamic slab accepts it and knocks the objects
    // resting on top into the air. Slab bottom is at y=500, reachable from
    // the floor with the ~200px jump. Jump under the slab to try it.
    spawnStatic(world, 452.0f, 530.0f, 24.0f, 60.0f, Color(0.40f, 0.50f, 0.40f));
    spawnStatic(world, 588.0f, 530.0f, 24.0f, 60.0f, Color(0.40f, 0.50f, 0.40f));
    spawnBox(world, 520.0f, 490.0f, 200.0f, 20.0f, Color(0.75f, 0.60f, 0.30f),
             0.2f, 1.0f);
    // Riders on top of the slab.
    spawnBox(world, 486.0f, 462.0f, 36.0f, 36.0f, Color(0.85f, 0.50f, 0.25f), 0.15f);
    spawnBox(world, 558.0f, 465.0f, 30.0f, 30.0f, Color(0.60f, 0.50f, 0.80f), 0.2f);
    spawnCircle(world, 524.0f, 462.0f, 18.0f, Color(0.30f, 0.80f, 0.55f), 0.35f);

    // Trampoline pad on the open floor: anything landing on it is launched.
    spawnTrampoline(world, 840.0f, 660.0f, 680.0f);

    // Spring pendulum swinging in the open sky at the left.
    spawnSpringPendulum(world, 150.0f, 250.0f, 205.0f, 350.0f, 18.0f);

    // Bouncy balls: restitution 0.75 (Box2D takes the max of the pair) plus a
    // bit of air drag, so they bounce lively but settle within a few seconds.
    spawnCircle(world, 300.0f, 100.0f, 20.0f, Color(0.95f, 0.30f, 0.60f),
                0.75f, 0.3f);
    spawnCircle(world, 1100.0f, 120.0f, 24.0f, Color(0.55f, 0.90f, 0.30f),
                0.75f, 0.3f);

    // Dynamic crates and balls
    spawnBox(world, 300.0f, 200.0f, 40.0f, 40.0f, Color(0.85f, 0.45f, 0.25f), 0.15f);
    spawnBox(world, 350.0f, 160.0f, 36.0f, 36.0f, Color(0.80f, 0.35f, 0.30f), 0.2f);
    spawnBox(world, 720.0f, 200.0f, 48.0f, 48.0f, Color(0.70f, 0.40f, 0.70f), 0.1f);
    spawnBox(world, 760.0f, 140.0f, 32.0f, 32.0f, Color(0.60f, 0.50f, 0.80f), 0.25f);
    spawnCircle(world, 660.0f, 180.0f, 22.0f, Color(0.95f, 0.55f, 0.20f), 0.35f);
    spawnCircle(world, 900.0f, 200.0f, 18.0f, Color(0.30f, 0.80f, 0.55f), 0.45f);
    spawnCircle(world, 1000.0f, 160.0f, 26.0f, Color(0.90f, 0.30f, 0.45f), 0.3f);

    // Sensor pickups
    spawnPickup(world, 320.0f, 470.0f);
    spawnPickup(world, 700.0f, 370.0f);
    spawnPickup(world, 520.0f, 425.0f);
    spawnPickup(world, 980.0f, 490.0f);

    spawnPlayer(world, 160.0f, 600.0f);

    rebuildSceneGraph();
    std::snprintf(statusBuf_, sizeof(statusBuf_),
                  "Pick up gold orbs | Score: 0");
    fprintf(stderr, "[PHYSICS] Scene loaded (%zu bodies)\n", visuals_.size());
}

void PhysicsScene::unload(World* world, ScriptSystem* script) {
    (void)script;
    visuals_.clear();
    playerBody_ = NULL;
    playerEntity_ = INVALID_ENTITY;
    physics_.shutdown();
    setRootNode(nullptr);
    if (world) world->clear();
    world_ = NULL;
}

void PhysicsScene::handleContacts() {
    const std::vector<ContactInfo>& begins = physics_.beginContacts();
    for (size_t i = 0; i < begins.size(); ++i) {
        const ContactInfo& c = begins[i];
        ++contactFlashCount_;

        Entity other = INVALID_ENTITY;
        if (c.entityA == playerEntity_) other = c.entityB;
        else if (c.entityB == playerEntity_) other = c.entityA;

        for (size_t v = 0; v < visuals_.size(); ++v) {
            BodyVisual& vis = visuals_[v];
            if (vis.entity == c.entityA || vis.entity == c.entityB) {
                if (!vis.isSensor) vis.flashTimer = 0.12f;
            }
        }

        // Trampoline launch: any dynamic body landing on top of a pad is
        // sent straight up. Bounce with energy loss: each rebound keeps 85%
        // of the pre-solve impact speed (capped at the launch speed), so an
        // idle body settles instead of bouncing forever.
        for (size_t t = 0; t < trampolines_.size(); ++t) {
            Trampoline& tr = trampolines_[t];
            b2Body* otherBody = NULL;
            Vec2 otherVel;
            if (c.entityA == tr.entity) {
                otherBody = c.bodyB;
                otherVel = c.velB;
            } else if (c.entityB == tr.entity) {
                otherBody = c.bodyA;
                otherVel = c.velA;
            }
            if (!otherBody || otherBody->GetType() != b2_dynamicBody) continue;

            // While the player holds DOWN on a pad they are charging, not
            // bouncing: absorb the landing instead of launching.
            if (otherBody == playerBody_) {
                InputSystem* input = App::instance().getInput();
                if (input && (input->isKeyDown(SDL_SCANCODE_S) ||
                              input->isKeyDown(SDL_SCANCODE_DOWN))) {
                    continue;
                }
            }

            Vec2 op = PhysicsSystem::getPosition(otherBody);
            float plateTop = tr.y - tr.h * 0.5f;
            if (op.y > plateTop) continue; // side/below contact, ignore

            float incoming = otherVel.y > 0.0f ? otherVel.y : 0.0f;
            float launch = incoming * 0.85f;
            if (launch > trampolineLaunchSpeed_) launch = trampolineLaunchSpeed_;
            PhysicsSystem::setLinearVelocity(otherBody, otherVel.x, -launch);
            if (launch > 50.0f) tr.compressTimer = 0.18f;
        }

        if (other == INVALID_ENTITY) continue;

        for (size_t v = 0; v < visuals_.size(); ++v) {
            BodyVisual& vis = visuals_[v];
            if (vis.entity != other || !vis.isSensor || vis.collected) continue;

            vis.collected = true;
            if (vis.body) {
                physics_.destroyBody(vis.body);
                vis.body = NULL;
                if (world_) {
                    RigidBodyComponent* rb =
                        world_->getComponent<RigidBodyComponent>(vis.entity);
                    if (rb) rb->body = NULL;
                }
            }
            ++score_;
            std::snprintf(statusBuf_, sizeof(statusBuf_),
                          "Collected! Score: %d", score_);
            fprintf(stderr, "[PHYSICS] Pickup collected, score=%d\n", score_);
        }
    }

    // Grounded check: the player counts as supported only when their center
    // of mass is within the support span of whatever they stand on (static
    // or dynamic). If the COM hangs past the edge they teeter outward
    // instead of magically balancing on the corner. Walls still don't count.
    // Grounded check runs even while toppling: the recovery below needs it.
    // Re-triggering a topple is guarded by !toppling_ at the trigger site.
    grounded_ = false;
    tipDir_ = 0;
    tipAmount_ = 0.0f;
    if (playerEntity_ != INVALID_ENTITY && playerBody_) {
        Vec2 pp = PhysicsSystem::getPosition(playerBody_);
        float playerBottom = pp.y + 24.0f; // half height
        for (size_t v = 0; v < visuals_.size(); ++v) {
            const BodyVisual& vis = visuals_[v];
            if (vis.entity == playerEntity_ || vis.isSensor || !vis.body) continue;
            if (!physics_.areTouching(playerEntity_, vis.entity)) continue;

            Vec2 sp = PhysicsSystem::getPosition(vis.body);
            float surfaceTop = sp.y - vis.h * 0.5f;
            if (playerBottom > surfaceTop + 8.0f) continue; // not on top

            // Support span at the top surface. A ball only offers a narrow
            // balance zone around its center.
            float halfSpan = vis.isCircle ? vis.w * 0.2f : vis.w * 0.5f;
            float rangeL = sp.x - halfSpan;
            float rangeR = sp.x + halfSpan;

            if (pp.x >= rangeL && pp.x <= rangeR) {
                grounded_ = true;
                tipDir_ = 0;
                tipAmount_ = 0.0f;
                break;
            }

            float over = (pp.x < rangeL) ? (rangeL - pp.x) : (pp.x - rangeR);
            int dir = (pp.x < rangeL) ? -1 : 1;
            if (over > tipAmount_) {
                tipAmount_ = over;
                tipDir_ = dir;
            }
        }
    }

    // Landed after a topple: snap upright and lock rotation again. The
    // upright box is taller than the tumbled one, so lift the center a bit
    // to keep the snap from digging into the support and slingshotting the
    // player out of the world.
    if (toppling_ && grounded_) {
        toppling_ = false;
        playerBody_->SetFixedRotation(true);
        playerBody_->SetAngularVelocity(0.0f);
        PhysicsSystem::setAngle(playerBody_, 0.0f);
        Vec2 pp = PhysicsSystem::getPosition(playerBody_);
        PhysicsSystem::setPosition(playerBody_, pp.x, pp.y - 6.0f);
    }
}

void PhysicsScene::syncVisuals() {
    if (!world_) return;

    for (size_t i = 0; i < visuals_.size(); ++i) {
        BodyVisual& v = visuals_[i];
        if (!v.body || v.collected) continue;

        Vec2 p = PhysicsSystem::getPosition(v.body);
        TransformComponent* t = world_->getComponent<TransformComponent>(v.entity);
        if (t) {
            t->transform.position = Vec3(p.x, p.y, 0);
        }

        Color c = v.flashTimer > 0.0f ? v.flashColor : v.baseColor;
        if (v.path) {
            // Rotated quad following the body's angle (used by the player).
            buildRotatedQuad(*v.path, p.x, p.y, v.w, v.h,
                             PhysicsSystem::getAngle(v.body), 0.0f);
            v.path->setFillColor(c);
        } else if (v.isCircle && v.ellipse) {
            v.ellipse->setCenter(p.x, p.y);
            v.ellipse->setFillColor(c);
        } else if (v.rect) {
            v.rect->setRect(p.x - v.w * 0.5f, p.y - v.h * 0.5f, v.w, v.h);
            v.rect->setColor(c);
        }
    }
}

void PhysicsScene::syncSpringVisuals() {
    // Pendulum coils follow their bobs.
    for (size_t i = 0; i < springs_.size(); ++i) {
        SpringPendulum& s = springs_[i];
        if (!s.coil || !s.anchorBody || !s.bobBody) continue;
        Vec2 a = PhysicsSystem::getPosition(s.anchorBody);
        Vec2 b = PhysicsSystem::getPosition(s.bobBody);
        buildCoil(*s.coil, a.x, a.y, b.x, b.y, 6, 7.0f);
    }

    // Trampoline coils, plus a short plate dip after a launch and a deeper
    // dip while the player is charging.
    for (size_t i = 0; i < trampolines_.size(); ++i) {
        Trampoline& tr = trampolines_[i];
        float dip = 0.0f;
        if (charging_ && chargePadIndex_ == (int)i) {
            dip = 4.0f + 8.0f * chargeTime_;
        } else if (tr.compressTimer > 0.0f) {
            dip = 5.0f * (tr.compressTimer / 0.18f);
        }
        if (tr.coil) {
            buildCoil(*tr.coil, tr.x, tr.surfaceY + dip, tr.x, tr.baseY,
                      4, 12.0f);
        }
        // Visual-only plate dip; the static body itself does not move.
        for (size_t vi = 0; vi < visuals_.size(); ++vi) {
            if (visuals_[vi].entity == tr.entity && visuals_[vi].rect) {
                visuals_[vi].rect->setRect(tr.x - tr.w * 0.5f,
                                           tr.y - tr.h * 0.5f + dip,
                                           tr.w, tr.h);
                break;
            }
        }

        // Keep the charging player visually seated on the dipped plate.
        if (dip > 0.0f && charging_ && chargePadIndex_ == (int)i) {
            for (size_t vi = 0; vi < visuals_.size(); ++vi) {
                if (visuals_[vi].entity != playerEntity_) continue;
                Vec2 pp = PhysicsSystem::getPosition(playerBody_);
                if (visuals_[vi].rect) {
                    visuals_[vi].rect->setRect(
                        pp.x - visuals_[vi].w * 0.5f,
                        pp.y - visuals_[vi].h * 0.5f + dip,
                        visuals_[vi].w, visuals_[vi].h);
                } else if (visuals_[vi].path) {
                    buildRotatedQuad(*visuals_[vi].path, pp.x, pp.y,
                                     visuals_[vi].w, visuals_[vi].h,
                                     PhysicsSystem::getAngle(playerBody_),
                                     dip);
                }
                break;
            }
        }
    }
}

void PhysicsScene::fixedUpdate() {
    // Physics stepped from update with frame dt (has its own 60Hz accumulator).
}

void PhysicsScene::update(double dt) {
    InputSystem* input = App::instance().getInput();
    if (input && input->isKeyPressed(SDL_SCANCODE_R)) {
        App::instance().getSceneManager()->setNext(new MenuScene());
        return;
    }

    // Player control
    if (playerBody_ && input) {
        float axis = 0.0f;
        if (input->isKeyDown(SDL_SCANCODE_A) || input->isKeyDown(SDL_SCANCODE_LEFT))
            axis -= 1.0f;
        if (input->isKeyDown(SDL_SCANCODE_D) || input->isKeyDown(SDL_SCANCODE_RIGHT))
            axis += 1.0f;

        // COM past the support edge: topple over the corner instead of
        // magically balancing on it. Rotation unlocks and gravity does the
        // rest; movement control is lost until landing.
        if (!toppling_ && tipDir_ != 0 && tipAmount_ > 4.0f) {
            toppling_ = true;
            playerBody_->SetFixedRotation(false);
            playerBody_->SetAngularVelocity(tipDir_ * 3.0f);
            std::snprintf(statusBuf_, sizeof(statusBuf_), "Lost balance!");
        }

        if (!toppling_) {
            Vec2 vel = PhysicsSystem::getLinearVelocity(playerBody_);
            PhysicsSystem::setLinearVelocity(playerBody_, axis * moveSpeed_,
                                             vel.y);
        }

        bool jump = input->isKeyPressed(SDL_SCANCODE_SPACE) ||
                    input->isKeyPressed(SDL_SCANCODE_W) ||
                    input->isKeyPressed(SDL_SCANCODE_UP);
        if (jump && grounded_) {
            PhysicsSystem::applyImpulse(playerBody_, 0.0f, -jumpImpulse_);
            grounded_ = false;
        }

        // Safety clamps: penetration correction during a tumble can slingshot
        // the player through walls or the floor.
        {
            Vec2 pv = PhysicsSystem::getLinearVelocity(playerBody_);
            const float maxV = 1400.0f;
            float cx = pv.x, cy = pv.y;
            if (cx > maxV) cx = maxV; else if (cx < -maxV) cx = -maxV;
            if (cy > maxV) cy = maxV; else if (cy < -maxV) cy = -maxV;
            if (cx != pv.x || cy != pv.y) {
                PhysicsSystem::setLinearVelocity(playerBody_, cx, cy);
            }
            float w = playerBody_->GetAngularVelocity();
            if (w > 12.0f) playerBody_->SetAngularVelocity(12.0f);
            else if (w < -12.0f) playerBody_->SetAngularVelocity(-12.0f);
        }

        // Out-of-world failsafe: bring the player back and restore control.
        {
            Vec2 pp = PhysicsSystem::getPosition(playerBody_);
            if (pp.y > kWorldH + 200.0f || pp.y < -400.0f ||
                pp.x < -200.0f || pp.x > kWorldW + 200.0f) {
                PhysicsSystem::setPosition(playerBody_, 160.0f, 600.0f);
                PhysicsSystem::setLinearVelocity(playerBody_, 0.0f, 0.0f);
                playerBody_->SetAngularVelocity(0.0f);
                playerBody_->SetFixedRotation(true);
                PhysicsSystem::setAngle(playerBody_, 0.0f);
                toppling_ = false;
                std::snprintf(statusBuf_, sizeof(statusBuf_), "Respawned");
            }
        }

        // Trampoline charge: hold S/Down while standing on a pad to store
        // energy (the pad stays compressed), release to launch higher than
        // a normal bounce. Walking off the pad cancels the charge.
        int padIndex = -1;
        for (size_t t = 0; t < trampolines_.size(); ++t) {
            Trampoline& tr = trampolines_[t];
            if (!tr.body) continue;
            if (!physics_.areTouching(playerEntity_, tr.entity)) continue;
            Vec2 pp = PhysicsSystem::getPosition(playerBody_);
            float plateTop = tr.y - tr.h * 0.5f;
            if (pp.y + 24.0f <= plateTop + 8.0f) {
                padIndex = (int)t;
                break;
            }
        }

        bool downHeld = input->isKeyDown(SDL_SCANCODE_S) ||
                        input->isKeyDown(SDL_SCANCODE_DOWN);
        if (downHeld && padIndex >= 0) {
            if (!charging_) {
                charging_ = true;
                chargeTime_ = 0.0f;
                chargePadIndex_ = padIndex;
            }
            chargeTime_ += static_cast<float>(dt);
            if (chargeTime_ > 1.0f) chargeTime_ = 1.0f;
        } else if (charging_) {
            if (padIndex >= 0 && padIndex == chargePadIndex_) {
                // 500 px/s for a tap, up to 1100 px/s at full charge.
                float launch = 500.0f + 600.0f * chargeTime_;
                Vec2 vel = PhysicsSystem::getLinearVelocity(playerBody_);
                PhysicsSystem::setLinearVelocity(playerBody_, vel.x, -launch);
                trampolines_[padIndex].compressTimer = 0.18f;
                std::snprintf(statusBuf_, sizeof(statusBuf_),
                              "Charged bounce! %.0f px/s", launch);
            }
            charging_ = false;
            chargeTime_ = 0.0f;
            chargePadIndex_ = -1;
        }

        // Click to drop a crate
        if (input->isMouseButtonPressed(SDL_BUTTON_LEFT) && world_) {
            float mx = input->getMouseX();
            float my = input->getMouseY();
            spawnBox(world_, mx, my, 34.0f, 34.0f,
                     Color(0.55f + 0.4f * (score_ % 3) / 2.0f,
                           0.40f, 0.70f, 1.0f),
                     0.3f);
            rebuildSceneGraph();
        }
    }

    physics_.step(dt);
    handleContacts();

    for (size_t i = 0; i < visuals_.size(); ++i) {
        if (visuals_[i].flashTimer > 0.0f) {
            visuals_[i].flashTimer -= static_cast<float>(dt);
            if (visuals_[i].flashTimer < 0.0f) visuals_[i].flashTimer = 0.0f;
        }
    }

    for (size_t i = 0; i < trampolines_.size(); ++i) {
        if (trampolines_[i].compressTimer > 0.0f) {
            trampolines_[i].compressTimer -= static_cast<float>(dt);
            if (trampolines_[i].compressTimer < 0.0f) {
                trampolines_[i].compressTimer = 0.0f;
            }
        }
    }

    // If any pickup was collected, rebuild graph to drop nodes
    bool needRebuild = false;
    for (size_t i = 0; i < visuals_.size(); ++i) {
        if (visuals_[i].collected && (visuals_[i].rect || visuals_[i].ellipse)) {
            needRebuild = true;
            break;
        }
    }
    // Also if node pointers were invalidated by rebuild already cleared
    if (needRebuild) {
        rebuildSceneGraph();
    } else {
        syncVisuals();
    }

    syncSpringVisuals();
}

bool PhysicsScene::buildClayUI(ClayUI& ui) {
    ClayUI::Box root;
    root.width = kWorldW;
    root.height = kWorldH;
    root.paddingT = 16;
    root.paddingL = 20;
    root.childGap = 6;
    // top-left stack without blocking full screen clicks for physics
    ui.beginBox(root);

    ClayUI::Box panel;
    panel.id = "PHYS_HUD";
    panel.width = 520.0f;
    panel.height = 90.0f;
    panel.background = Color(0.0f, 0.0f, 0.0f, 0.45f);
    panel.cornerRadius = 8.0f;
    panel.paddingT = 10;
    panel.paddingL = 14;
    panel.paddingR = 14;
    panel.childGap = 4;
    ui.beginBox(panel);

    char line1[96];
    std::snprintf(line1, sizeof(line1),
                  "Physics Demo  |  Score: %d  |  Bodies: %zu",
                  score_, visuals_.size());
    ui.text(line1, 18, Color(1, 1, 1, 1));
    ui.text(statusBuf_, 16, Color(0.85f, 0.9f, 1.0f, 1));
    ui.text("WASD move  Space jump  LMB box  S charge bounce  R menu", 14,
            Color(0.7f, 0.75f, 0.8f, 1));

    ui.endBox();
    ui.endBox();
    return true;
}
