#include "second_scene.h"

#include "domi/app.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "domi/input.h"
#include "domi/script.h"
#include "domi/scene_manager.h"
#include "domi/render_node.h"
#include "game2d_scene.h"

using namespace domi;

SecondScene::SecondScene() : blockEntity_(0) {}

void SecondScene::load(World* world, ScriptSystem* script) {
    blockEntity_ = world->createEntity();
    world->addComponent<TransformComponent>(blockEntity_);
    world->addComponent<SpriteComponent>(blockEntity_)->color = Color(1.0f, 0.2f, 0.2f, 1.0f);

    if (script) {
        script->loadScript(blockEntity_, "scripts/player_2d.wasm");
    }
}

void SecondScene::unload(World* world, ScriptSystem* script) {
    if (script && blockEntity_ != 0) {
        script->unloadScript(blockEntity_);
        blockEntity_ = 0;
    }
    if (world) world->clear();
}

void SecondScene::update(double dt) {
    (void)dt;
    if (App::instance().getInput()->isKeyPressed(SDL_SCANCODE_R)) {
        App::instance().getSceneManager()->setNext(new Game2DScene());
    }
}
