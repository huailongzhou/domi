#include "game3d_scene.h"

#include "domi/app.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "domi/input.h"
#include "domi/math.h"
#include "domi/scene_manager.h"
#include "domi/render_node.h"
#include "game2d_scene.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

using namespace domi;

Game3DScene::Game3DScene() : player_(0), score_(0) {
    std::srand(static_cast<unsigned>(std::time(NULL)));
}

void Game3DScene::load(World* world, ScriptSystem* script) {
    (void)script;

    // Ground cube platform
    Entity ground = world->createEntity();
    TransformComponent* groundT = world->addComponent<TransformComponent>(ground);
    groundT->transform.position = Vec3(0.0f, -0.4f, 0.0f);
    groundT->transform.scale = Vec3(3.0f, 0.8f, 3.0f);
    MeshComponent* groundMesh = world->addComponent<MeshComponent>(ground);
    groundMesh->modelPath = "builtin:cube";
    groundMesh->color = Color(0.35f, 0.35f, 0.35f);

    // Player cube
    player_ = world->createEntity();
    TransformComponent* playerT = world->addComponent<TransformComponent>(player_);
    playerT->transform.position = Vec3(0.0f, 0.35f, 0.0f);
    playerT->transform.scale = Vec3(0.35f, 0.35f, 0.35f);
    MeshComponent* playerMesh = world->addComponent<MeshComponent>(player_);
    playerMesh->modelPath = "builtin:cube";
    playerMesh->color = Color(0.2f, 0.6f, 1.0f);

    // Camera
    Entity camera = world->createEntity();
    TransformComponent* camT = world->addComponent<TransformComponent>(camera);
    camT->transform.position = Vec3(5.0f, 5.0f, 5.0f);
    CameraComponent* cam = world->addComponent<CameraComponent>(camera);
    cam->isPerspective = false;
    cam->size = 3.5f;
    cam->isActive = true;

    spawnCollectibles(world);
    score_ = 0;
}

void Game3DScene::unload(World* world, ScriptSystem* script) {
    (void)script;
    if (world) world->clear();
    collectibles_.clear();
    score_ = 0;
}

void Game3DScene::update(double dt) {
    World* world = App::instance().getWorld();
    InputSystem* input = App::instance().getInput();
    if (!world || !input) return;

    const float moveSpeed = 4.0f;
    const float limit = 2.5f;
    float h = input->getAxis("Horizontal");
    float v = input->getAxis("Vertical");

    TransformComponent* pt = world->getComponent<TransformComponent>(player_);
    if (pt) {
        pt->transform.position.x += h * moveSpeed * dt;
        pt->transform.position.z += v * moveSpeed * dt;
        if (pt->transform.position.x < -limit) pt->transform.position.x = -limit;
        if (pt->transform.position.x >  limit) pt->transform.position.x =  limit;
        if (pt->transform.position.z < -limit) pt->transform.position.z = -limit;
        if (pt->transform.position.z >  limit) pt->transform.position.z =  limit;
    }

    float time = App::instance().getTime();
    for (size_t i = 0; i < collectibles_.size(); ++i) {
        Entity e = collectibles_[i];
        if (!world->isValid(e)) continue;
        TransformComponent* t = world->getComponent<TransformComponent>(e);
        if (t) {
            float angle = -(time * 2.0f + static_cast<float>(i));
            t->transform.rotation = Quat::fromEuler(0.0f, angle, 0.0f);
        }
    }

    if (pt) {
        Vec3 ppos = pt->transform.position;
        for (size_t i = 0; i < collectibles_.size();) {
            Entity e = collectibles_[i];
            if (!world->isValid(e)) {
                collectibles_.erase(collectibles_.begin() + i);
                continue;
            }
            TransformComponent* t = world->getComponent<TransformComponent>(e);
            if (!t) {
                collectibles_.erase(collectibles_.begin() + i);
                continue;
            }
            float dx = ppos.x - t->transform.position.x;
            float dz = ppos.z - t->transform.position.z;
            if (dx * dx + dz * dz < 0.3f * 0.3f) {
                world->destroyEntity(e);
                collectibles_.erase(collectibles_.begin() + i);
                ++score_;
                fprintf(stderr, "[GAME] Score: %d\n", score_);
            } else {
                ++i;
            }
        }
    }

    if (collectibles_.empty()) {
        spawnCollectibles(world);
        fprintf(stderr, "[GAME] New wave! Score: %d\n", score_);
    }

    if (input->isKeyPressed(SDL_SCANCODE_R)) {
        App::instance().getSceneManager()->setNext(new Game2DScene());
    }
}

void Game3DScene::spawnCollectibles(World* world) {
    const int count = 5;
    for (int i = 0; i < count; ++i) {
        Entity c = world->createEntity();
        TransformComponent* t = world->addComponent<TransformComponent>(c);
        float radius = 0.8f + (std::rand() % 1200) / 1000.0f;
        float angle = (std::rand() % 6283) / 1000.0f;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        t->transform.position = Vec3(x, 0.3f, z);
        t->transform.scale = Vec3(0.25f, 0.25f, 0.25f);
        MeshComponent* collectibleMesh = world->addComponent<MeshComponent>(c);
        collectibleMesh->modelPath = "builtin:cube";
        collectibleMesh->color = Color(1.0f, 0.9f, 0.15f);
        collectibles_.push_back(c);
    }
}
