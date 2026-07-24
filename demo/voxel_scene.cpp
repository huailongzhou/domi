#include "voxel_scene.h"

#include "domi/app.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "domi/input.h"
#include "domi/math.h"
#include "domi/scene_manager.h"
#include "game2d_scene.h"
#include "domi/render_node.h"
#include <cstdio>
#include <cmath>

using namespace domi;

VoxelScene::VoxelScene()
    : camera_(0), terrain_(0), cameraAngle_(0.6f), cameraHeight_(12.0f) {}

void VoxelScene::load(World* world, ScriptSystem* script) {
    (void)script;

    // A single voxel chunk used as a height-map terrain.
    const int width  = 32;
    const int height = 16;
    const int depth  = 32;

    Color palette[5];
    palette[0] = Color(0.0f, 0.0f, 0.0f, 0.0f); // empty
    palette[1] = Color(0.3f, 0.8f, 0.2f);        // grass
    palette[2] = Color(0.6f, 0.4f, 0.2f);        // dirt
    palette[3] = Color(0.5f, 0.5f, 0.5f);        // stone
    palette[4] = Color(0.9f, 0.8f, 0.4f);        // sand

    auto terrainHeight = [&](int x, int z) -> int {
        float fx = x * 0.25f;
        float fz = z * 0.25f;
        float h = 6.0f
                + 4.0f * std::sin(fx)
                + 3.0f * std::cos(fz)
                + 2.0f * std::sin((fx + fz) * 0.5f);
        int ih = (int)std::floor(h);
        if (ih < 0) ih = 0;
        if (ih >= height) ih = height - 1;
        return ih;
    };

    terrain_ = world->createEntity();
    TransformComponent* tc = world->addComponent<TransformComponent>(terrain_);
    tc->transform.position = Vec3(
        -width  * 0.5f,
        -height * 0.25f,
        -depth  * 0.5f);

    VoxelComponent* vc = world->addComponent<VoxelComponent>(terrain_);
    vc->width  = width;
    vc->height = height;
    vc->depth  = depth;
    vc->palette.assign(palette, palette + 5);
    vc->voxels.assign(width * height * depth, 0);

    auto setVoxel = [&](int x, int y, int z, uint8_t index) {
        if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= depth) return;
        vc->voxels[(z * height + y) * width + x] = index;
    };

    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            int surface = terrainHeight(x, z);
            for (int y = 0; y <= surface; ++y) {
                uint8_t index;
                if (y == surface) {
                    index = 1; // grass
                } else if (y >= surface - 2) {
                    index = 2; // dirt
                } else {
                    index = 3; // stone
                }
                setVoxel(x, y, z, index);
            }
        }
    }

    // Perspective camera looking at the terrain center.
    camera_ = world->createEntity();
    TransformComponent* ct = world->addComponent<TransformComponent>(camera_);
    const float radius = 30.0f;
    ct->transform.position = Vec3(
        std::cos(cameraAngle_) * radius,
        cameraHeight_,
        std::sin(cameraAngle_) * radius);
    ct->transform.rotation = Quat::fromEuler(
        -cameraAngle_ - 3.14159265f * 0.5f, 0.0f, 0.0f);

    CameraComponent* cam = world->addComponent<CameraComponent>(camera_);
    cam->isPerspective = true;
    cam->fov = 60.0f * 3.14159265f / 180.0f;
    cam->nearPlane = 0.1f;
    cam->farPlane = 200.0f;
    cam->isActive = true;
}

void VoxelScene::unload(World* world, ScriptSystem* script) {
    (void)script;
    if (world) world->clear();
    camera_ = 0;
    terrain_ = 0;
}

void VoxelScene::update(double dt) {
    World* world = App::instance().getWorld();
    InputSystem* input = App::instance().getInput();
    if (!world || !input) return;

    const float rotateSpeed = 1.0f;
    const float heightSpeed = 4.0f;

    if (input->isKeyDown(SDL_SCANCODE_LEFT))  cameraAngle_ -= rotateSpeed * dt;
    if (input->isKeyDown(SDL_SCANCODE_RIGHT)) cameraAngle_ += rotateSpeed * dt;
    if (input->isKeyDown(SDL_SCANCODE_UP))    cameraHeight_ += heightSpeed * dt;
    if (input->isKeyDown(SDL_SCANCODE_DOWN))  cameraHeight_ -= heightSpeed * dt;

    const float radius = 30.0f;
    TransformComponent* ct = world->getComponent<TransformComponent>(camera_);
    if (ct) {
        ct->transform.position = Vec3(
            std::cos(cameraAngle_) * radius,
            cameraHeight_,
            std::sin(cameraAngle_) * radius);
        ct->transform.rotation = Quat::fromEuler(
            -cameraAngle_ - 3.14159265f * 0.5f, 0.0f, 0.0f);
    }

    if (input->isKeyPressed(SDL_SCANCODE_R)) {
        App::instance().getSceneManager()->setNext(new Game2DScene());
    }
}
