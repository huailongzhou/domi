#ifndef DOMI_SCRIPT_H
#define DOMI_SCRIPT_H

#include "domi/types.h"
#include "domi/thread_pool.h"
#include <vector>
#include <string>

// Forward declarations for WAMR
struct WASMModuleCommon;
struct WASMModuleInstanceCommon;
struct WASMExecEnv;
struct wasm_runtime_init_args_t;

namespace domi {

class World;

struct ScriptInstance {
    Entity entity;
    std::string path;
    void* module;      // WASMModuleCommon*
    void* instance;    // WASMModuleInstanceCommon*
    void* execEnv;     // WASMExecEnv*
    bool hasInit;
    bool hasUpdate;
    bool hasFixedUpdate;
    bool hasCleanup;
};

class ScriptSystem {
public:
    ScriptSystem();
    ~ScriptSystem();

    bool init();
    void shutdown();

    // Load a WASM script for an entity
    bool loadScript(Entity entity, const char* wasmPath);
    void unloadScript(Entity entity);

    void update(double dt);
    void fixedUpdate();

    void setThreadPool(ThreadPool* pool) { threadPool_ = pool; }

    // Native API functions exposed to WASM
    static uint32_t api_entityCreate();
    static void api_entityDestroy(uint32_t entity);
    static void api_transformSetPosition(uint32_t entity, float x, float y, float z);
    static void api_transformGetPosition(uint32_t entity, float* x, float* y, float* z);
    static void api_transformSetRotation(uint32_t entity, float x, float y, float z, float w);
    static void api_transformSetScale(uint32_t entity, float x, float y, float z);
    static void api_spriteSetTexture(uint32_t entity, const char* path);
    static void api_spriteSetColor(uint32_t entity, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    static void api_meshSetModel(uint32_t entity, const char* path);
    static void api_cameraSetPerspective(uint32_t entity, float fov, float near, float far);
    static void api_cameraSetActive(uint32_t entity);
    static void api_lightSetType(uint32_t entity, int type);
    static void api_lightSetColor(uint32_t entity, float r, float g, float b);
    static void api_lightSetIntensity(uint32_t entity, float intensity);
    static int api_inputIsKeyPressed(int key);
    static int api_inputIsKeyDown(int key);
    static float api_inputGetAxis(const char* axis);
    static void api_audioPlay(const char* path, int loop);
    static void api_audioStop(const char* path);
    static void api_log(const char* message);

private:
    std::vector<ScriptInstance> scripts_;
    bool runtimeInitialized_;
    ThreadPool* threadPool_;

    bool callFunction(ScriptInstance& s, const char* name, uint32_t argc, uint32_t* argv);
    ScriptInstance* findScript(Entity entity);
};

} // namespace domi

#endif
