#include "domi/script.h"
#include "domi/app.h"
#include "domi/input.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "domi/thread_pool.h"
#include "wasm_export.h"
#include <cstdio>
#include <cstring>
#include <mutex>

namespace domi {

// Global mutex protecting all native API calls that touch shared engine state.
static std::mutex g_scriptApiMutex;

static inline float get_f32(uint64_t v) {
    float f;
    memcpy(&f, &v, sizeof(float));
    return f;
}

static inline void set_f32(uint64_t* v, float f) {
    memcpy(v, &f, sizeof(float));
}

static inline const char* app_to_native(wasm_exec_env_t exec_env, uint32_t offset) {
    if (offset == 0) return NULL;
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    return (const char*)wasm_runtime_addr_app_to_native(inst, offset);
}

static inline void* app_to_native_ptr(wasm_exec_env_t exec_env, uint32_t offset) {
    if (offset == 0) return NULL;
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    return wasm_runtime_addr_app_to_native(inst, offset);
}

// Raw native function wrappers for WAMR
static void wasm_entity_create(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    args[0] = ScriptSystem::api_entityCreate();
}

static void wasm_entity_destroy(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_entityDestroy((uint32_t)args[0]);
}

static void wasm_transform_set_pos(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_transformSetPosition(
        (uint32_t)args[0], get_f32(args[1]), get_f32(args[2]), get_f32(args[3]));
}

static void wasm_transform_get_pos(wasm_exec_env_t exec_env, uint64_t* args) {
    float x, y, z;
    ScriptSystem::api_transformGetPosition((uint32_t)args[0], &x, &y, &z);
    float* px = (float*)app_to_native_ptr(exec_env, (uint32_t)args[1]);
    float* py = (float*)app_to_native_ptr(exec_env, (uint32_t)args[2]);
    float* pz = (float*)app_to_native_ptr(exec_env, (uint32_t)args[3]);
    if (px) *px = x;
    if (py) *py = y;
    if (pz) *pz = z;
}

static void wasm_transform_set_rot(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_transformSetRotation(
        (uint32_t)args[0], get_f32(args[1]), get_f32(args[2]), get_f32(args[3]), get_f32(args[4]));
}

static void wasm_transform_set_scale(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_transformSetScale(
        (uint32_t)args[0], get_f32(args[1]), get_f32(args[2]), get_f32(args[3]));
}

static void wasm_sprite_set_tex(wasm_exec_env_t exec_env, uint64_t* args) {
    ScriptSystem::api_spriteSetTexture((uint32_t)args[0], app_to_native(exec_env, (uint32_t)args[1]));
}

static void wasm_sprite_set_color(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_spriteSetColor(
        (uint32_t)args[0], (uint8_t)args[1], (uint8_t)args[2], (uint8_t)args[3], (uint8_t)args[4]);
}

static void wasm_mesh_set_model(wasm_exec_env_t exec_env, uint64_t* args) {
    ScriptSystem::api_meshSetModel((uint32_t)args[0], app_to_native(exec_env, (uint32_t)args[1]));
}

static void wasm_camera_set_persp(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_cameraSetPerspective(
        (uint32_t)args[0], get_f32(args[1]), get_f32(args[2]), get_f32(args[3]));
}

static void wasm_camera_set_active(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_cameraSetActive((uint32_t)args[0]);
}

static void wasm_light_set_type(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_lightSetType((uint32_t)args[0], (int32_t)args[1]);
}

static void wasm_light_set_color(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_lightSetColor(
        (uint32_t)args[0], get_f32(args[1]), get_f32(args[2]), get_f32(args[3]));
}

static void wasm_light_set_intensity(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    ScriptSystem::api_lightSetIntensity((uint32_t)args[0], get_f32(args[1]));
}

static void wasm_input_key_pressed(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    args[0] = ScriptSystem::api_inputIsKeyPressed((int32_t)args[0]);
}

static void wasm_input_key_down(wasm_exec_env_t exec_env, uint64_t* args) {
    (void)exec_env;
    args[0] = ScriptSystem::api_inputIsKeyDown((int32_t)args[0]);
}

static void wasm_input_get_axis(wasm_exec_env_t exec_env, uint64_t* args) {
    float ret = ScriptSystem::api_inputGetAxis(app_to_native(exec_env, (uint32_t)args[0]));
    set_f32(&args[0], ret);
}

static void wasm_audio_play(wasm_exec_env_t exec_env, uint64_t* args) {
    ScriptSystem::api_audioPlay(app_to_native(exec_env, (uint32_t)args[0]), (int32_t)args[1]);
}

static void wasm_audio_stop(wasm_exec_env_t exec_env, uint64_t* args) {
    ScriptSystem::api_audioStop(app_to_native(exec_env, (uint32_t)args[0]));
}

static void wasm_log(wasm_exec_env_t exec_env, uint64_t* args) {
    ScriptSystem::api_log(app_to_native(exec_env, (uint32_t)args[0]));
}

// Native symbol registration (raw API - signatures are placeholders)
static NativeSymbol native_symbols[] = {
    {"domi_entity_create", (void*)wasm_entity_create, NULL, NULL},
    {"domi_entity_destroy", (void*)wasm_entity_destroy, NULL, NULL},
    {"domi_transform_set_position", (void*)wasm_transform_set_pos, NULL, NULL},
    {"domi_transform_get_position", (void*)wasm_transform_get_pos, NULL, NULL},
    {"domi_transform_set_rotation", (void*)wasm_transform_set_rot, NULL, NULL},
    {"domi_transform_set_scale", (void*)wasm_transform_set_scale, NULL, NULL},
    {"domi_sprite_set_texture", (void*)wasm_sprite_set_tex, NULL, NULL},
    {"domi_sprite_set_color", (void*)wasm_sprite_set_color, NULL, NULL},
    {"domi_mesh_set_model", (void*)wasm_mesh_set_model, NULL, NULL},
    {"domi_camera_set_perspective", (void*)wasm_camera_set_persp, NULL, NULL},
    {"domi_camera_set_active", (void*)wasm_camera_set_active, NULL, NULL},
    {"domi_light_set_type", (void*)wasm_light_set_type, NULL, NULL},
    {"domi_light_set_color", (void*)wasm_light_set_color, NULL, NULL},
    {"domi_light_set_intensity", (void*)wasm_light_set_intensity, NULL, NULL},
    {"domi_input_is_key_pressed", (void*)wasm_input_key_pressed, NULL, NULL},
    {"domi_input_is_key_down", (void*)wasm_input_key_down, NULL, NULL},
    {"domi_input_get_axis", (void*)wasm_input_get_axis, NULL, NULL},
    {"domi_audio_play", (void*)wasm_audio_play, NULL, NULL},
    {"domi_audio_stop", (void*)wasm_audio_stop, NULL, NULL},
    {"domi_log", (void*)wasm_log, NULL, NULL},
};

static void* wasm_malloc_wrapper(uint32_t size) { return malloc(size); }
static void* wasm_realloc_wrapper(void* ptr, uint32_t size) { return realloc(ptr, size); }
static void wasm_free_wrapper(void* ptr) { free(ptr); }

ScriptSystem::ScriptSystem() : runtimeInitialized_(false), threadPool_(NULL) {}

ScriptSystem::~ScriptSystem() {
    shutdown();
}

bool ScriptSystem::init() {
    if (runtimeInitialized_) return true;

    RuntimeInitArgs initArgs;
    memset(&initArgs, 0, sizeof(initArgs));
    initArgs.mem_alloc_type = Alloc_With_Allocator;
    initArgs.mem_alloc_option.allocator.malloc_func = (void*)wasm_malloc_wrapper;
    initArgs.mem_alloc_option.allocator.realloc_func = (void*)wasm_realloc_wrapper;
    initArgs.mem_alloc_option.allocator.free_func = (void*)wasm_free_wrapper;

    if (!wasm_runtime_full_init(&initArgs)) {
        fprintf(stderr, "WAMR runtime init failed\n");
        return false;
    }

    uint32_t n_symbols = sizeof(native_symbols) / sizeof(NativeSymbol);
    if (!wasm_runtime_register_natives_raw("env", native_symbols, n_symbols)) {
        fprintf(stderr, "WAMR register natives failed\n");
        wasm_runtime_destroy();
        return false;
    }

    runtimeInitialized_ = true;
    return true;
}

void ScriptSystem::shutdown() {
    for (size_t i = 0; i < scripts_.size(); ++i) {
        if (scripts_[i].instance) {
            wasm_runtime_deinstantiate((WASMModuleInstanceCommon*)scripts_[i].instance);
        }
        if (scripts_[i].module) {
            wasm_runtime_unload((WASMModuleCommon*)scripts_[i].module);
        }
    }
    scripts_.clear();

    if (runtimeInitialized_) {
        wasm_runtime_destroy();
        runtimeInitialized_ = false;
    }
}

bool ScriptSystem::loadScript(Entity entity, const char* wasmPath) {
    if (!runtimeInitialized_) return false;

    // Read WASM file
    uint8_t* wasmBuffer = NULL;
    uint32_t wasmSize = 0;
    FILE* f = fopen(wasmPath, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open WASM file: %s\n", wasmPath);
        return false;
    }
    fseek(f, 0, SEEK_END);
    wasmSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    wasmBuffer = (uint8_t*)malloc(wasmSize);
    fread(wasmBuffer, 1, wasmSize, f);
    fclose(f);

    // Load module with a copy of const strings so the original buffer can be
    // freed immediately.
    LoadArgs loadArgs;
    memset(&loadArgs, 0, sizeof(loadArgs));
    loadArgs.wasm_binary_freeable = true;

    WASMModuleCommon* module = wasm_runtime_load_ex(wasmBuffer, wasmSize, &loadArgs, NULL, 0);
    free(wasmBuffer);
    if (!module) {
        fprintf(stderr, "Failed to load WASM module\n");
        return false;
    }

    // Instantiate using the newer API
    InstantiationArgs2* instArgs = NULL;
    char errorBuf[128];
    if (!wasm_runtime_instantiation_args_create(&instArgs)) {
        fprintf(stderr, "Failed to create instantiation args\n");
        wasm_runtime_unload(module);
        return false;
    }
    wasm_runtime_instantiation_args_set_default_stack_size(instArgs, 64 * 1024);
    wasm_runtime_instantiation_args_set_host_managed_heap_size(instArgs, 16 * 1024);

    WASMModuleInstanceCommon* instance = wasm_runtime_instantiate_ex2(module, instArgs, errorBuf, sizeof(errorBuf));
    wasm_runtime_instantiation_args_destroy(instArgs);
    if (!instance) {
        fprintf(stderr, "Failed to instantiate WASM: %s\n", errorBuf);
        wasm_runtime_unload(module);
        return false;
    }

    ScriptInstance s;
    s.entity = entity;
    s.path = wasmPath;
    s.module = module;
    s.instance = instance;
    s.execEnv = wasm_runtime_create_exec_env(instance, 16 * 1024);
    s.hasInit = wasm_runtime_lookup_function(instance, "script_init") != NULL;
    s.hasUpdate = wasm_runtime_lookup_function(instance, "script_update") != NULL;
    s.hasFixedUpdate = wasm_runtime_lookup_function(instance, "script_fixed_update") != NULL;
    s.hasCleanup = wasm_runtime_lookup_function(instance, "script_cleanup") != NULL;

    scripts_.push_back(s);

    // Call init
    if (s.hasInit) {
        if (!callFunction(s, "script_init", 0, NULL)) {
            const char* exception = wasm_runtime_get_exception((WASMModuleInstanceCommon*)s.instance);
            fprintf(stderr, "script_init failed: %s\n", exception ? exception : "unknown");
        }
    }

    return true;
}

void ScriptSystem::unloadScript(Entity entity) {
    for (size_t i = 0; i < scripts_.size(); ++i) {
        if (scripts_[i].entity == entity) {
            if (scripts_[i].hasCleanup) {
                callFunction(scripts_[i], "script_cleanup", 0, NULL);
            }
            if (scripts_[i].execEnv) wasm_runtime_destroy_exec_env((WASMExecEnv*)scripts_[i].execEnv);
            if (scripts_[i].instance) wasm_runtime_deinstantiate((WASMModuleInstanceCommon*)scripts_[i].instance);
            if (scripts_[i].module) wasm_runtime_unload((WASMModuleCommon*)scripts_[i].module);
            scripts_.erase(scripts_.begin() + i);
            return;
        }
    }
}

void ScriptSystem::update(double dt) {
    if (threadPool_ && !scripts_.empty()) {
        const float dtFloat = static_cast<float>(dt);
        for (size_t i = 0; i < scripts_.size(); ++i) {
            if (scripts_[i].hasUpdate) {
                ScriptInstance* s = &scripts_[i];
                threadPool_->submit([this, s, dtFloat]() {
                    uint32_t argv[1];
                    memcpy(argv, &dtFloat, sizeof(float));
                    callFunction(*s, "script_update", 1, argv);
                });
            }
        }
        threadPool_->wait();
        return;
    }

    for (size_t i = 0; i < scripts_.size(); ++i) {
        if (scripts_[i].hasUpdate) {
            uint32_t argv[1];
            memcpy(argv, &dt, sizeof(float));
            callFunction(scripts_[i], "script_update", 1, argv);
        }
    }
}

void ScriptSystem::fixedUpdate() {
    for (size_t i = 0; i < scripts_.size(); ++i) {
        if (scripts_[i].hasFixedUpdate) {
            callFunction(scripts_[i], "script_fixed_update", 0, NULL);
        }
    }
}

bool ScriptSystem::callFunction(ScriptInstance& s, const char* name, uint32_t argc, uint32_t* argv) {
    WASMFunctionInstanceCommon* func = wasm_runtime_lookup_function((WASMModuleInstanceCommon*)s.instance, name);
    if (!func) return false;
    return wasm_runtime_call_wasm((WASMExecEnv*)s.execEnv, func, argc, argv);
}

ScriptInstance* ScriptSystem::findScript(Entity entity) {
    for (size_t i = 0; i < scripts_.size(); ++i) {
        if (scripts_[i].entity == entity) return &scripts_[i];
    }
    return NULL;
}

// --- Native API implementations ---

uint32_t ScriptSystem::api_entityCreate() {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    Entity e = world->createEntity();
    world->addComponent<TransformComponent>(e);
    return e;
}

void ScriptSystem::api_entityDestroy(uint32_t entity) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    App::instance().getWorld()->destroyEntity(entity);
}

void ScriptSystem::api_transformSetPosition(uint32_t entity, float x, float y, float z) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    TransformComponent* t = world->getComponent<TransformComponent>(entity);
    if (!t) t = world->addComponent<TransformComponent>(entity);
    if (t) t->transform.position = Vec3(x, y, z);
}

void ScriptSystem::api_transformGetPosition(uint32_t entity, float* x, float* y, float* z) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    TransformComponent* t = App::instance().getWorld()->getComponent<TransformComponent>(entity);
    if (t && x && y && z) {
        *x = t->transform.position.x;
        *y = t->transform.position.y;
        *z = t->transform.position.z;
    }
}

void ScriptSystem::api_transformSetRotation(uint32_t entity, float x, float y, float z, float w) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    TransformComponent* t = world->getComponent<TransformComponent>(entity);
    if (!t) t = world->addComponent<TransformComponent>(entity);
    if (t) t->transform.rotation = Quat(x, y, z, w);
}

void ScriptSystem::api_transformSetScale(uint32_t entity, float x, float y, float z) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    TransformComponent* t = world->getComponent<TransformComponent>(entity);
    if (!t) t = world->addComponent<TransformComponent>(entity);
    if (t) t->transform.scale = Vec3(x, y, z);
}

void ScriptSystem::api_spriteSetTexture(uint32_t entity, const char* path) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    SpriteComponent* s = world->getComponent<SpriteComponent>(entity);
    if (!s) s = world->addComponent<SpriteComponent>(entity);
    if (s) s->texturePath = path;
}

void ScriptSystem::api_spriteSetColor(uint32_t entity, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    SpriteComponent* s = world->getComponent<SpriteComponent>(entity);
    if (!s) s = world->addComponent<SpriteComponent>(entity);
    if (s) s->color = Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void ScriptSystem::api_meshSetModel(uint32_t entity, const char* path) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    MeshComponent* m = world->getComponent<MeshComponent>(entity);
    if (!m) m = world->addComponent<MeshComponent>(entity);
    if (m) m->modelPath = path;
}

void ScriptSystem::api_cameraSetPerspective(uint32_t entity, float fov, float near, float far) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    CameraComponent* c = world->getComponent<CameraComponent>(entity);
    if (!c) c = world->addComponent<CameraComponent>(entity);
    if (c) { c->isPerspective = true; c->fov = fov; c->nearPlane = near; c->farPlane = far; }
}

void ScriptSystem::api_cameraSetActive(uint32_t entity) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    CameraComponent* c = world->getComponent<CameraComponent>(entity);
    if (!c) c = world->addComponent<CameraComponent>(entity);
    if (c) c->isActive = true;
}

void ScriptSystem::api_lightSetType(uint32_t entity, int type) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    LightComponent* l = world->getComponent<LightComponent>(entity);
    if (!l) l = world->addComponent<LightComponent>(entity);
    if (l) l->type = (LightComponent::Type)type;
}

void ScriptSystem::api_lightSetColor(uint32_t entity, float r, float g, float b) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    LightComponent* l = world->getComponent<LightComponent>(entity);
    if (!l) l = world->addComponent<LightComponent>(entity);
    if (l) l->color = Color(r, g, b);
}

void ScriptSystem::api_lightSetIntensity(uint32_t entity, float intensity) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    World* world = App::instance().getWorld();
    LightComponent* l = world->getComponent<LightComponent>(entity);
    if (!l) l = world->addComponent<LightComponent>(entity);
    if (l) l->intensity = intensity;
}

int ScriptSystem::api_inputIsKeyPressed(int key) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    return App::instance().getInput()->isKeyPressed(key) ? 1 : 0;
}

int ScriptSystem::api_inputIsKeyDown(int key) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    return App::instance().getInput()->isKeyDown(key) ? 1 : 0;
}

float ScriptSystem::api_inputGetAxis(const char* axis) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    return App::instance().getInput()->getAxis(axis);
}

void ScriptSystem::api_audioPlay(const char* path, int loop) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    (void)path; (void)loop;
}

void ScriptSystem::api_audioStop(const char* path) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    (void)path;
}

void ScriptSystem::api_log(const char* message) {
    std::lock_guard<std::mutex> lock(g_scriptApiMutex);
    fprintf(stderr, "[SCRIPT] %s\n", message ? message : "(null)");
}

} // namespace domi
