#ifndef DOMI_COMPONENT_H
#define DOMI_COMPONENT_H

#include "domi/types.h"
#include "domi/math.h"
#include <string>

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

} // namespace domi

#endif
