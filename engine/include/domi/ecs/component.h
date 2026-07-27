#ifndef DOMI_COMPONENT_H
#define DOMI_COMPONENT_H

#include "domi/core/types.h"
#include "domi/core/math.h"
#include <string>
#include <vector>

namespace domi {

struct TransformComponent {
    Transform transform;
};

struct SpriteComponent {
    std::string texturePath;
    Rect sourceRect;
    Color color;
    bool flipX;
    bool flipY;
    bool castShadow;
    SpriteComponent() : sourceRect(), color(1,1,1,1), flipX(false), flipY(false), castShadow(true) {}
};

struct MeshComponent {
    std::string modelPath;
    std::string materialPath;
    Color color;
    MeshComponent() : color(1, 1, 1, 1) {}
};

struct CameraComponent {
    bool isPerspective;
    float fov;      // perspective: vertical fov in radians
    float size;     // orthographic: half-size
    float nearPlane;
    float farPlane;
    bool isActive;
    CameraComponent() : isPerspective(true), fov(1.047f), size(5.0f),
                        nearPlane(0.1f), farPlane(100.0f), isActive(false) {}
};

struct LightComponent {
    enum Type { Directional = 0, Point = 1, Spot = 2 };
    Type type;
    Color color;
    float intensity;
    LightComponent() : type(Directional), color(1,1,1), intensity(1.0f) {}
};

struct ScriptComponent {
    std::string wasmPath;
    void* runtimeInstance; // WAMR module instance
    ScriptComponent() : runtimeInstance(NULL) {}
};

struct AudioSourceComponent {
    std::string path;
    bool loop;
    bool playOnStart;
    float volume;
    AudioSourceComponent() : loop(false), playOnStart(false), volume(1.0f) {}
};

struct VoxelComponent {
    int width;
    int height;
    int depth;
    std::vector<uint8_t> voxels; // palette indices, 0 = empty voxel
    std::vector<Color> palette;  // index 0 reserved for empty
    VoxelComponent() : width(0), height(0), depth(0) {}
};

// 2D rigid body driven by PhysicsSystem / Box2D.
// body is owned by PhysicsSystem; destroy via PhysicsSystem::destroyBody.
struct RigidBodyComponent {
    void* body;          // b2Body*
    int shape;           // 0=box, 1=circle (ShapeType)
    float width;         // pixels (box w or circle diameter)
    float height;        // pixels (box h; unused for circle)
    bool isSensor;
    uint16_t categoryBits;
    uint16_t maskBits;
    RigidBodyComponent()
        : body(NULL), shape(0), width(0), height(0), isSensor(false),
          categoryBits(0x0001), maskBits(0xFFFF) {}
};

} // namespace domi

#endif
