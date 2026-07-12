#ifndef DOMI_SCENE_MANAGER_H
#define DOMI_SCENE_MANAGER_H

#include "domi/scene.h"
#include <cstddef>

namespace domi {

class World;
class ScriptSystem;

// Manages scene lifetime and transitions.
// The manager takes ownership of scenes passed to setNext() and deletes them
// when they are replaced or when the manager is destroyed.
class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    // Queue a scene to be loaded at the start of the next frame.
    // The manager takes ownership of the pointer.
    void setNext(Scene* scene);

    Scene* current() const { return current_; }

    // Performs pending scene transitions and updates the active scene.
    void update(World* world, ScriptSystem* script, double dt);

    // Fixed-timestep update for the active scene.
    void fixedUpdate(ScriptSystem* script);

    // Render the active scene via Canvas2D.
    void render(Canvas2D* canvas);

    // Unload the current scene immediately.
    void clear(World* world, ScriptSystem* script);

private:
    Scene* current_;
    Scene* next_;

    SceneManager(const SceneManager&);
    SceneManager& operator=(const SceneManager&);
};

} // namespace domi

#endif
