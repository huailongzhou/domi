#ifndef DOMI_SCENE_H
#define DOMI_SCENE_H

#include "domi/types.h"
#include <memory>

namespace domi {

class World;
class ScriptSystem;
class Canvas2D;
class RenderList;
class RenderNode;
class UIView;
class UIContext;

// A Scene is a self-contained collection of entities/components.
// Derive from this class and override load/unload to build your level.
class Scene {
public:
    virtual ~Scene();

    // Called when the scene becomes active. Create entities here.
    virtual void load(World* world, ScriptSystem* script) = 0;

    // Called when the scene is replaced or the app shuts down.
    // Remove scene-specific entities if you don't want to clear the whole world.
    virtual void unload(World* world, ScriptSystem* script) = 0;

    // Optional per-frame scene logic (runs before script updates).
    virtual void update(double dt) { (void)dt; }

    // Optional fixed-timestep scene logic.
    virtual void fixedUpdate() {}

    // Optional scene rendering via a declarative RenderList (2D path only).
    // The default implementation builds the root render node, if one was set.
    virtual void render(RenderList& list);

    // Set the root of the 2D render node tree. The tree is built once
    // (typically in load()) and its properties can be animated in update().
    void setRootNode(std::unique_ptr<RenderNode> root);
    RenderNode* getRootNode() const { return rootNode_.get(); }

    // Optional scene name for debugging.
    virtual const char* name() const { return "Scene"; }

    // Optional declarative UI root/context. When both are non-null the
    // UIPass will render this view tree on top of the composited frame.
    virtual UIView* getUIRoot() { return nullptr; }
    virtual UIContext* getUIContext() { return nullptr; }

private:
    std::unique_ptr<RenderNode> rootNode_;
};

} // namespace domi

#endif
