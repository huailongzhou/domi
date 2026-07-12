#ifndef DOMI_SCENE_H
#define DOMI_SCENE_H

#include "domi/types.h"

namespace domi {

class World;
class ScriptSystem;
class Canvas2D;

// A Scene is a self-contained collection of entities/components.
// Derive from this class and override load/unload to build your level.
class Scene {
public:
    virtual ~Scene() {}

    // Called when the scene becomes active. Create entities here.
    virtual void load(World* world, ScriptSystem* script) = 0;

    // Called when the scene is replaced or the app shuts down.
    // Remove scene-specific entities if you don't want to clear the whole world.
    virtual void unload(World* world, ScriptSystem* script) = 0;

    // Optional per-frame scene logic (runs before script updates).
    virtual void update(double dt) { (void)dt; }

    // Optional fixed-timestep scene logic.
    virtual void fixedUpdate() {}

    // Optional scene rendering via Canvas2D (2D path only).
    virtual void render(Canvas2D* canvas) { (void)canvas; }

    // Optional scene name for debugging.
    virtual const char* name() const { return "Scene"; }
};

} // namespace domi

#endif
