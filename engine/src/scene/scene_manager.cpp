#include "domi/scene_manager.h"
#include "domi/ecs.h"
#include "domi/script.h"
#include "domi/render_list.h"
#include <cstdio>

namespace domi {

SceneManager::SceneManager() : current_(NULL), next_(NULL) {}

SceneManager::~SceneManager() {
    delete next_;
    delete current_;
}

void SceneManager::setNext(Scene* scene) {
    delete next_;
    next_ = scene;
}

void SceneManager::update(World* world, ScriptSystem* script, double dt) {
    if (next_) {
        if (current_) {
            fprintf(stderr, "[SCENE] Unloading '%s'\n", current_->name());
            current_->unload(world, script);
            delete current_;
        }
        current_ = next_;
        next_ = NULL;
        if (current_) {
            fprintf(stderr, "[SCENE] Loading '%s'\n", current_->name());
            current_->load(world, script);
        }
    }

    if (current_) {
        current_->update(dt);
    }
}

void SceneManager::fixedUpdate(ScriptSystem* script) {
    (void)script;
    if (current_) {
        current_->fixedUpdate();
    }
}

void SceneManager::render(RenderList& list) {
    if (current_) {
        current_->render(list);
    }
}

void SceneManager::clear(World* world, ScriptSystem* script) {
    if (current_) {
        current_->unload(world, script);
        delete current_;
        current_ = NULL;
    }
    delete next_;
    next_ = NULL;
}

} // namespace domi
