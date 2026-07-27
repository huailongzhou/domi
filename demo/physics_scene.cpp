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
      jumpImpulse_(420.0f), grounded_(false) {
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
                            const Color& color, float restitution) {
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
    def.density = 1.0f;
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
                               const Color& color, float restitution) {
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
    def.linearDamping = 0.2f;
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

    setRootNode(std::move(root));
}

void PhysicsScene::load(World* world, ScriptSystem* script) {
    (void)script;
    world_ = world;
    visuals_.clear();
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
    spawnStatic(world, 520.0f, 300.0f, 160.0f, 24.0f, Color(0.40f, 0.50f, 0.40f));

    // Dynamic crates and balls
    spawnBox(world, 300.0f, 200.0f, 40.0f, 40.0f, Color(0.85f, 0.45f, 0.25f), 0.15f);
    spawnBox(world, 350.0f, 160.0f, 36.0f, 36.0f, Color(0.80f, 0.35f, 0.30f), 0.2f);
    spawnBox(world, 720.0f, 200.0f, 48.0f, 48.0f, Color(0.70f, 0.40f, 0.70f), 0.1f);
    spawnBox(world, 760.0f, 140.0f, 32.0f, 32.0f, Color(0.60f, 0.50f, 0.80f), 0.25f);
    spawnCircle(world, 500.0f, 180.0f, 22.0f, Color(0.95f, 0.55f, 0.20f), 0.35f);
    spawnCircle(world, 900.0f, 200.0f, 18.0f, Color(0.30f, 0.80f, 0.55f), 0.45f);
    spawnCircle(world, 1000.0f, 160.0f, 26.0f, Color(0.90f, 0.30f, 0.45f), 0.3f);

    // Sensor pickups
    spawnPickup(world, 320.0f, 470.0f);
    spawnPickup(world, 700.0f, 370.0f);
    spawnPickup(world, 520.0f, 250.0f);
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

    // Grounded check: player touching any non-sensor, non-self body below-ish
    grounded_ = false;
    if (playerEntity_ != INVALID_ENTITY) {
        for (size_t i = 0; i < physics_.beginContacts().size(); ++i) {
            // use active contacts via isTouching against statics
        }
        // Check active contacts through areTouching with static platforms
        for (size_t v = 0; v < visuals_.size(); ++v) {
            const BodyVisual& vis = visuals_[v];
            if (vis.entity == playerEntity_ || vis.isSensor || !vis.body) continue;
            if (vis.body->GetType() != b2_staticBody) continue;
            if (physics_.areTouching(playerEntity_, vis.entity)) {
                // Require contact normal roughly upward (player on top)
                // Approximate: player center above platform top
                Vec2 pp = PhysicsSystem::getPosition(playerBody_);
                Vec2 sp = PhysicsSystem::getPosition(vis.body);
                float platformTop = sp.y - vis.h * 0.5f;
                float playerBottom = pp.y + 24.0f; // half height
                if (playerBottom <= platformTop + 8.0f) {
                    grounded_ = true;
                    break;
                }
            }
        }
        // Also floor
        if (!grounded_) {
            for (size_t v = 0; v < visuals_.size(); ++v) {
                const BodyVisual& vis = visuals_[v];
                if (!vis.body || vis.isSensor) continue;
                if (vis.body->GetType() != b2_staticBody) continue;
                if (physics_.areTouching(playerEntity_, vis.entity)) {
                    grounded_ = true;
                    break;
                }
            }
        }
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
        if (v.isCircle && v.ellipse) {
            v.ellipse->setCenter(p.x, p.y);
            v.ellipse->setFillColor(c);
        } else if (v.rect) {
            v.rect->setRect(p.x - v.w * 0.5f, p.y - v.h * 0.5f, v.w, v.h);
            v.rect->setColor(c);
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

        Vec2 vel = PhysicsSystem::getLinearVelocity(playerBody_);
        PhysicsSystem::setLinearVelocity(playerBody_, axis * moveSpeed_, vel.y);

        bool jump = input->isKeyPressed(SDL_SCANCODE_SPACE) ||
                    input->isKeyPressed(SDL_SCANCODE_W) ||
                    input->isKeyPressed(SDL_SCANCODE_UP);
        if (jump && grounded_) {
            PhysicsSystem::applyImpulse(playerBody_, 0.0f, -jumpImpulse_);
            grounded_ = false;
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
    ui.text("WASD move  Space jump  LMB spawn box  R menu", 14,
            Color(0.7f, 0.75f, 0.8f, 1));

    ui.endBox();
    ui.endBox();
    return true;
}
