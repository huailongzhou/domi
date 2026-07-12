#ifndef DOMI_ECS_H
#define DOMI_ECS_H

#include "domi/types.h"
#include "domi/component.h"
#include <vector>

namespace domi {

struct ComponentTypeMask {
    uint32_t mask;
    ComponentTypeMask() : mask(0) {}
    ComponentTypeMask& withTransform() { mask |= 1; return *this; }
    ComponentTypeMask& withSprite() { mask |= 2; return *this; }
    ComponentTypeMask& withMesh() { mask |= 4; return *this; }
    ComponentTypeMask& withCamera() { mask |= 8; return *this; }
    ComponentTypeMask& withLight() { mask |= 16; return *this; }
    ComponentTypeMask& withScript() { mask |= 32; return *this; }
    ComponentTypeMask& withAudio() { mask |= 64; return *this; }
};

class World {
public:
    World();
    ~World();

    Entity createEntity();
    void destroyEntity(Entity e);
    bool isValid(Entity e) const;

    // Destroy all entities and clear all component pools.
    void clear();

    template<typename T>
    T* addComponent(Entity e) {
        ensureCapacity<T>(e);
        T* pool = getComponentPool<T>();
        if (!pool) return NULL;
        pool[e] = T();
        return &pool[e];
    }

    template<typename T>
    T* getComponent(Entity e) {
        T* pool = getComponentPool<T>();
        if (!pool || e >= getPoolSize<T>()) return NULL;
        return &pool[e];
    }

    template<typename T>
    bool hasComponent(Entity e) {
        return getComponent<T>(e) != NULL;
    }

    template<typename T>
    void removeComponent(Entity e) {
        T* pool = getComponentPool<T>();
        if (pool && e < getPoolSize<T>()) {
            pool[e] = T();
        }
    }

    std::vector<Entity> queryEntitiesWith(ComponentTypeMask mask);

private:
    Entity nextEntity_;
    std::vector<bool> alive_;

    std::vector<TransformComponent> transforms_;
    std::vector<SpriteComponent> sprites_;
    std::vector<MeshComponent> meshes_;
    std::vector<CameraComponent> cameras_;
    std::vector<LightComponent> lights_;
    std::vector<ScriptComponent> scripts_;
    std::vector<AudioSourceComponent> audios_;

    template<typename T> T* getComponentPool();
    template<typename T> size_t getPoolSize() const;
    template<typename T> void ensureCapacity(Entity e);
};

} // namespace domi

#endif
