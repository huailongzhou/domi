#include "domi/ecs.h"

namespace domi {

World::World() : nextEntity_(1) {}

World::~World() {}

Entity World::createEntity() {
    Entity e = nextEntity_++;
    if (e >= alive_.size()) alive_.resize(e + 1, false);
    alive_[e] = true;
    return e;
}

void World::destroyEntity(Entity e) {
    if (e < alive_.size()) alive_[e] = false;
}

bool World::isValid(Entity e) const {
    return e < alive_.size() && alive_[e];
}

void World::clear() {
    alive_.clear();
    alive_.resize(1, false);
    transforms_.clear();
    sprites_.clear();
    meshes_.clear();
    cameras_.clear();
    lights_.clear();
    scripts_.clear();
    audios_.clear();
    nextEntity_ = 1;
}

std::vector<Entity> World::queryEntitiesWith(ComponentTypeMask mask) {
    std::vector<Entity> result;
    for (Entity e = 1; e < alive_.size(); ++e) {
        if (!alive_[e]) continue;
        uint32_t has = 0;
        if (e < transforms_.size() && transforms_[e].transform.scale.x != 0) has |= 1;
        if (e < sprites_.size() && !sprites_[e].texturePath.empty()) has |= 2;
        if (e < meshes_.size() && !meshes_[e].modelPath.empty()) has |= 4;
        if (e < cameras_.size()) has |= 8;
        if (e < lights_.size()) has |= 16;
        if (e < scripts_.size() && !scripts_[e].wasmPath.empty()) has |= 32;
        if (e < audios_.size() && !audios_[e].path.empty()) has |= 64;
        if ((has & mask.mask) == mask.mask) result.push_back(e);
    }
    return result;
}

} // namespace domi

// Template specializations - must be in .cpp to avoid duplicate symbols
template<> domi::TransformComponent* domi::World::getComponentPool<domi::TransformComponent>() { return transforms_.empty() ? NULL : &transforms_[0]; }
template<> domi::SpriteComponent* domi::World::getComponentPool<domi::SpriteComponent>() { return sprites_.empty() ? NULL : &sprites_[0]; }
template<> domi::MeshComponent* domi::World::getComponentPool<domi::MeshComponent>() { return meshes_.empty() ? NULL : &meshes_[0]; }
template<> domi::CameraComponent* domi::World::getComponentPool<domi::CameraComponent>() { return cameras_.empty() ? NULL : &cameras_[0]; }
template<> domi::LightComponent* domi::World::getComponentPool<domi::LightComponent>() { return lights_.empty() ? NULL : &lights_[0]; }
template<> domi::ScriptComponent* domi::World::getComponentPool<domi::ScriptComponent>() { return scripts_.empty() ? NULL : &scripts_[0]; }
template<> domi::AudioSourceComponent* domi::World::getComponentPool<domi::AudioSourceComponent>() { return audios_.empty() ? NULL : &audios_[0]; }

template<> size_t domi::World::getPoolSize<domi::TransformComponent>() const { return transforms_.size(); }
template<> size_t domi::World::getPoolSize<domi::SpriteComponent>() const { return sprites_.size(); }
template<> size_t domi::World::getPoolSize<domi::MeshComponent>() const { return meshes_.size(); }
template<> size_t domi::World::getPoolSize<domi::CameraComponent>() const { return cameras_.size(); }
template<> size_t domi::World::getPoolSize<domi::LightComponent>() const { return lights_.size(); }
template<> size_t domi::World::getPoolSize<domi::ScriptComponent>() const { return scripts_.size(); }
template<> size_t domi::World::getPoolSize<domi::AudioSourceComponent>() const { return audios_.size(); }

template<> void domi::World::ensureCapacity<domi::TransformComponent>(Entity e) { if (e >= transforms_.size()) transforms_.resize(e + 1); }
template<> void domi::World::ensureCapacity<domi::SpriteComponent>(Entity e) { if (e >= sprites_.size()) sprites_.resize(e + 1); }
template<> void domi::World::ensureCapacity<domi::MeshComponent>(Entity e) { if (e >= meshes_.size()) meshes_.resize(e + 1); }
template<> void domi::World::ensureCapacity<domi::CameraComponent>(Entity e) { if (e >= cameras_.size()) cameras_.resize(e + 1); }
template<> void domi::World::ensureCapacity<domi::LightComponent>(Entity e) { if (e >= lights_.size()) lights_.resize(e + 1); }
template<> void domi::World::ensureCapacity<domi::ScriptComponent>(Entity e) { if (e >= scripts_.size()) scripts_.resize(e + 1); }
template<> void domi::World::ensureCapacity<domi::AudioSourceComponent>(Entity e) { if (e >= audios_.size()) audios_.resize(e + 1); }
