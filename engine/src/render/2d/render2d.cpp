#include "domi/render.h"
#include "domi/backend/render_backend.h"
#include "domi/backend/window_backend.h"
#include "domi/component.h"
#include "domi/math.h"
#include "domi/canvas2d.h"
#include "domi/scene_manager.h"
#include "domi/scene.h"
#include "domi/renderer.h"
#include "domi/pass/geometry_pass.h"
#include "domi/pass/shadow_pass.h"
#include "domi/pass/lighting_pass.h"
#include "domi/pass/composite_pass.h"
#include "domi/pass/ui_pass.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace domi {

namespace {

struct VertexData {
    float x, y, z;
    float r, g, b;
};

static const VertexData cubeVertices[] = {
    // Front face (red tint base)
    { -0.5f,  0.5f, -0.5f, 1.0f, 0.2f, 0.2f },
    {  0.5f, -0.5f, -0.5f, 1.0f, 0.2f, 0.2f },
    { -0.5f, -0.5f, -0.5f, 1.0f, 0.2f, 0.2f },
    { -0.5f,  0.5f, -0.5f, 1.0f, 0.2f, 0.2f },
    {  0.5f,  0.5f, -0.5f, 1.0f, 0.2f, 0.2f },
    {  0.5f, -0.5f, -0.5f, 1.0f, 0.2f, 0.2f },
    // Left face (green tint base)
    { -0.5f,  0.5f,  0.5f, 0.2f, 1.0f, 0.2f },
    { -0.5f, -0.5f, -0.5f, 0.2f, 1.0f, 0.2f },
    { -0.5f, -0.5f,  0.5f, 0.2f, 1.0f, 0.2f },
    { -0.5f,  0.5f,  0.5f, 0.2f, 1.0f, 0.2f },
    { -0.5f,  0.5f, -0.5f, 0.2f, 1.0f, 0.2f },
    { -0.5f, -0.5f, -0.5f, 0.2f, 1.0f, 0.2f },
    // Top face (white)
    { -0.5f,  0.5f,  0.5f, 0.9f, 0.9f, 0.9f },
    {  0.5f,  0.5f, -0.5f, 0.9f, 0.9f, 0.9f },
    { -0.5f,  0.5f, -0.5f, 0.9f, 0.9f, 0.9f },
    { -0.5f,  0.5f,  0.5f, 0.9f, 0.9f, 0.9f },
    {  0.5f,  0.5f,  0.5f, 0.9f, 0.9f, 0.9f },
    {  0.5f,  0.5f, -0.5f, 0.9f, 0.9f, 0.9f },
    // Right face (yellow tint base)
    {  0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.2f },
    {  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.2f },
    {  0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.2f },
    {  0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.2f },
    {  0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.2f },
    {  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.2f },
    // Back face (magenta tint base)
    {  0.5f,  0.5f,  0.5f, 1.0f, 0.2f, 1.0f },
    { -0.5f, -0.5f,  0.5f, 1.0f, 0.2f, 1.0f },
    {  0.5f, -0.5f,  0.5f, 1.0f, 0.2f, 1.0f },
    {  0.5f,  0.5f,  0.5f, 1.0f, 0.2f, 1.0f },
    { -0.5f,  0.5f,  0.5f, 1.0f, 0.2f, 1.0f },
    { -0.5f, -0.5f,  0.5f, 1.0f, 0.2f, 1.0f },
    // Bottom face (cyan tint base)
    { -0.5f, -0.5f, -0.5f, 0.2f, 1.0f, 1.0f },
    {  0.5f, -0.5f,  0.5f, 0.2f, 1.0f, 1.0f },
    { -0.5f, -0.5f,  0.5f, 0.2f, 1.0f, 1.0f },
    { -0.5f, -0.5f, -0.5f, 0.2f, 1.0f, 1.0f },
    {  0.5f, -0.5f, -0.5f, 0.2f, 1.0f, 1.0f },
    {  0.5f, -0.5f,  0.5f, 0.2f, 1.0f, 1.0f }
};

static const char* cubeVertexMSL =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct VSOutput\n"
"{\n"
"    float4 color [[user(locn0)]];\n"
"    float4 position [[position]];\n"
"};\n"
"\n"
"struct UBO\n"
"{\n"
"    float4x4 modelViewProj;\n"
"};\n"
"\n"
"struct VSInput\n"
"{\n"
"    float3 position [[attribute(0)]];\n"
"    float3 color [[attribute(1)]];\n"
"};\n"
"\n"
"vertex VSOutput vs_main(VSInput input [[stage_in]], constant UBO& ubo [[buffer(0)]])\n"
"{\n"
"    VSOutput output;\n"
"    float3 normal = normalize(input.position);\n"
"    float3 lightDir = normalize(float3(0.5, -1.0, -0.5));\n"
"    float diff = saturate(dot(normal, -lightDir)) * 0.7 + 0.3;\n"
"    output.color = float4(input.color * diff, 1.0);\n"
"    output.position = ubo.modelViewProj * float4(input.position, 1.0);\n"
"    return output;\n"
"}\n";

static const char* cubeFragmentMSL =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct VSOutput\n"
"{\n"
"    float4 color [[user(locn0)]];\n"
"    float4 position [[position]];\n"
"};\n"
"\n"
"fragment float4 fs_main(VSOutput input [[stage_in]])\n"
"{\n"
"    return input.color;\n"
"}\n";

static void rotate_matrix(float angle, float x, float y, float z, float* r) {
    float radians = angle * 3.14159265f / 180.0f;
    float c = SDL_cosf(radians);
    float s = SDL_sinf(radians);
    float c1 = 1.0f - c;
    float length = SDL_sqrtf(x * x + y * y + z * z);
    float u[3] = { x / length, y / length, z / length };

    for (int i = 0; i < 16; i++) r[i] = 0.0f;
    r[15] = 1.0f;

    for (int i = 0; i < 3; i++) {
        r[i * 4 + (i + 1) % 3] = u[(i + 2) % 3] * s;
        r[i * 4 + (i + 2) % 3] = -u[(i + 1) % 3] * s;
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r[i * 4 + j] += c1 * u[i] * u[j] + (i == j ? c : 0.0f);
        }
    }
}

static void perspective_matrix(float fovy, float aspect, float znear, float zfar, float* r) {
    float f = 1.0f / SDL_tanf(fovy * 0.5f);
    for (int i = 0; i < 16; i++) r[i] = 0.0f;
    r[0] = f / aspect;
    r[5] = f;
    r[10] = (znear + zfar) / (znear - zfar);
    r[11] = -1.0f;
    r[14] = (2.0f * znear * zfar) / (znear - zfar);
    r[15] = 0.0f;
}

static void multiply_matrix(const float* lhs, const float* rhs, float* r) {
    float tmp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[j * 4 + i] = 0.0f;
            for (int k = 0; k < 4; k++) {
                tmp[j * 4 + i] += lhs[k * 4 + i] * rhs[j * 4 + k];
            }
        }
    }
    for (int i = 0; i < 16; i++) r[i] = tmp[i];
}

} // anonymous namespace

RenderSystem::RenderSystem(IRenderBackend* renderBackend,
                             IWindowBackend* windowBackend)
    : renderBackend_(renderBackend), windowBackend_(windowBackend),
      useGPU_(false), canvas_(NULL), renderer_(NULL),
      gpu3DInited_(false), cubeVertexBuffer_(NULL), dynamicVertexBuffer_(NULL),
      cubePipeline_(NULL), depthTexture_(NULL), drawableW_(0), drawableH_(0),
      angleX_(0), angleY_(0), angleZ_(0), dynamicVertexCapacity_(0) {}

RenderSystem::~RenderSystem() {
    shutdown();
}

bool RenderSystem::init() {
    if (!renderBackend_ || !windowBackend_) return false;
    useGPU_ = (windowBackend_->getNativeGPUDevice() != NULL);
    renderer_ = new Renderer();
    if (!renderer_->init(renderBackend_, windowBackend_->getWidth(), windowBackend_->getHeight())) {
        delete renderer_;
        renderer_ = NULL;
        return false;
    }
    canvas_ = renderer_->getCanvas2D();

    // Build the default 2D deferred/compositing pipeline.
    renderer_->addPass(new GeometryPass(NULL));
    renderer_->addPass(new ShadowPass());
    renderer_->addPass(new LightingPass());
    renderer_->addPass(new CompositePass());
    renderer_->addPass(new UIPass());
    return true;
}

void RenderSystem::shutdown() {
    shutdownGPU3D();
    delete renderer_;
    renderer_ = NULL;
    canvas_ = NULL;
}

void RenderSystem::render(World* world, SceneManager* sceneManager) {
    if (!renderBackend_ || !windowBackend_) return;

    // Check whether any entity wants 3D rendering.
    bool has3D = !world->queryEntitiesWith(ComponentTypeMask().withMesh()).empty();

    if (has3D) {
        // 3D path: claim the GPU window once and keep it until 2D is needed.
        if (!windowBackend_->isGPUClaimed()) {
            if (!windowBackend_->claimGPU()) {
                fprintf(stderr, "[RENDER] Failed to claim GPU for 3D\n");
                return;
            }
            fprintf(stderr, "[RENDER] Switched to GPU 3D rendering\n");
        }
        render3D(world);
    } else {
        // 2D path: release GPU if it was claimed, then use RenderPass pipeline.
        if (windowBackend_->isGPUClaimed()) {
            windowBackend_->releaseGPU();
        }
        if (renderer_) {
            // Make sure the GeometryPass has the current SceneManager.
            for (size_t i = 0; i < renderer_->getPassCount(); ++i) {
                GeometryPass* gp = dynamic_cast<GeometryPass*>(renderer_->getPass(i));
                if (gp && gp->getSceneManager() != sceneManager) {
                    gp->setSceneManager(sceneManager);
                }
            }
            renderer_->render(world, sceneManager);
        }
    }
}

void RenderSystem::render2D(World* world) {
    // Legacy immediate-mode 2D path. The engine now uses the RenderPass pipeline
    // via Renderer::render(); this function is kept for compatibility but does
    // nothing by default.
    (void)world;
}

bool RenderSystem::initGPU3D() {
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(windowBackend_->getNativeGPUDevice());
    if (!device) return false;

    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(device);
    if (!(format & SDL_GPU_SHADERFORMAT_MSL)) {
        fprintf(stderr, "[RENDER3D] MSL shader format not supported\n");
        return false;
    }

    // Create vertex shader
    SDL_GPUShaderCreateInfo vsInfo;
    SDL_zero(vsInfo);
    vsInfo.code = (const Uint8*)cubeVertexMSL;
    vsInfo.code_size = SDL_strlen(cubeVertexMSL);
    vsInfo.entrypoint = "vs_main";
    vsInfo.format = SDL_GPU_SHADERFORMAT_MSL;
    vsInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vsInfo.num_uniform_buffers = 1;
    SDL_GPUShader* vs = SDL_CreateGPUShader(device, &vsInfo);
    if (!vs) {
        fprintf(stderr, "[RENDER3D] Failed to create vertex shader: %s\n", SDL_GetError());
        return false;
    }

    // Create fragment shader
    SDL_GPUShaderCreateInfo fsInfo;
    SDL_zero(fsInfo);
    fsInfo.code = (const Uint8*)cubeFragmentMSL;
    fsInfo.code_size = SDL_strlen(cubeFragmentMSL);
    fsInfo.entrypoint = "fs_main";
    fsInfo.format = SDL_GPU_SHADERFORMAT_MSL;
    fsInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    SDL_GPUShader* fs = SDL_CreateGPUShader(device, &fsInfo);
    if (!fs) {
        fprintf(stderr, "[RENDER3D] Failed to create fragment shader: %s\n", SDL_GetError());
        SDL_ReleaseGPUShader(device, vs);
        return false;
    }

    // Create vertex buffer
    SDL_GPUBufferCreateInfo bufferDesc;
    SDL_zero(bufferDesc);
    bufferDesc.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferDesc.size = sizeof(cubeVertices);
    cubeVertexBuffer_ = SDL_CreateGPUBuffer(device, &bufferDesc);
    if (!cubeVertexBuffer_) {
        fprintf(stderr, "[RENDER3D] Failed to create vertex buffer: %s\n", SDL_GetError());
        SDL_ReleaseGPUShader(device, vs);
        SDL_ReleaseGPUShader(device, fs);
        return false;
    }

    // Upload vertex data via transfer buffer
    SDL_GPUTransferBufferCreateInfo transferDesc;
    SDL_zero(transferDesc);
    transferDesc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferDesc.size = sizeof(cubeVertices);
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferDesc);
    if (transfer) {
        void* map = SDL_MapGPUTransferBuffer(device, transfer, false);
        if (map) {
            SDL_memcpy(map, cubeVertices, sizeof(cubeVertices));
            SDL_UnmapGPUTransferBuffer(device, transfer);
        }
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTransferBufferLocation loc;
        loc.transfer_buffer = transfer;
        loc.offset = 0;
        SDL_GPUBufferRegion region;
        region.buffer = cubeVertexBuffer_;
        region.offset = 0;
        region.size = sizeof(cubeVertices);
        SDL_UploadToGPUBuffer(copyPass, &loc, &region, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
    }

    // Create a dynamic vertex buffer large enough for many transformed cubes.
    {
        const size_t maxCubes = 1024;
        SDL_GPUBufferCreateInfo dynDesc;
        SDL_zero(dynDesc);
        dynDesc.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        dynDesc.size = sizeof(cubeVertices) * maxCubes;
        dynamicVertexBuffer_ = SDL_CreateGPUBuffer(device, &dynDesc);
        if (!dynamicVertexBuffer_) {
            fprintf(stderr, "[RENDER3D] Failed to create dynamic vertex buffer: %s\n", SDL_GetError());
            SDL_ReleaseGPUShader(device, vs);
            SDL_ReleaseGPUShader(device, fs);
            SDL_ReleaseGPUBuffer(device, cubeVertexBuffer_);
            cubeVertexBuffer_ = NULL;
            return false;
        }
        dynamicVertexCapacity_ = maxCubes;
    }

    // Get swapchain format for color target
    SDL_GPUTextureFormat swapFormat = SDL_GetGPUSwapchainTextureFormat(device, static_cast<SDL_Window*>(windowBackend_->getNativeWindow()));

    // Create pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipeInfo;
    SDL_zero(pipeInfo);

    SDL_GPUColorTargetDescription colorTarget;
    SDL_zero(colorTarget);
    colorTarget.format = swapFormat;

    pipeInfo.target_info.num_color_targets = 1;
    pipeInfo.target_info.color_target_descriptions = &colorTarget;
    pipeInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    pipeInfo.target_info.has_depth_stencil_target = true;

    pipeInfo.depth_stencil_state.enable_depth_test = true;
    pipeInfo.depth_stencil_state.enable_depth_write = true;
    pipeInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    pipeInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeInfo.vertex_shader = vs;
    pipeInfo.fragment_shader = fs;

    SDL_GPUVertexBufferDescription vbufDesc;
    SDL_zero(vbufDesc);
    vbufDesc.slot = 0;
    vbufDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vbufDesc.pitch = sizeof(VertexData);

    SDL_GPUVertexAttribute attrs[2];
    SDL_zero(attrs);
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].location = 0;
    attrs[0].offset = 0;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].location = 1;
    attrs[1].offset = sizeof(float) * 3;

    pipeInfo.vertex_input_state.num_vertex_buffers = 1;
    pipeInfo.vertex_input_state.vertex_buffer_descriptions = &vbufDesc;
    pipeInfo.vertex_input_state.num_vertex_attributes = 2;
    pipeInfo.vertex_input_state.vertex_attributes = attrs;

    cubePipeline_ = SDL_CreateGPUGraphicsPipeline(device, &pipeInfo);

    SDL_ReleaseGPUShader(device, vs);
    SDL_ReleaseGPUShader(device, fs);

    if (!cubePipeline_) {
        fprintf(stderr, "[RENDER3D] Failed to create pipeline: %s\n", SDL_GetError());
        return false;
    }

    // Create initial depth texture
    int w, h;
    SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(windowBackend_->getNativeWindow()), &w, &h);
    if (!createDepthTexture(w, h)) {
        return false;
    }

    gpu3DInited_ = true;
    fprintf(stderr, "[RENDER3D] GPU 3D pipeline initialized\n");
    return true;
}

bool RenderSystem::createDepthTexture(int w, int h) {
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(windowBackend_->getNativeGPUDevice());
    if (!device) return false;

    SDL_ReleaseGPUTexture(device, depthTexture_);
    depthTexture_ = NULL;

    SDL_GPUTextureCreateInfo depthInfo;
    SDL_zero(depthInfo);
    depthInfo.type = SDL_GPU_TEXTURETYPE_2D;
    depthInfo.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    depthInfo.width = (Uint32)w;
    depthInfo.height = (Uint32)h;
    depthInfo.layer_count_or_depth = 1;
    depthInfo.num_levels = 1;
    depthInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    depthInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthTexture_ = SDL_CreateGPUTexture(device, &depthInfo);
    if (!depthTexture_) {
        fprintf(stderr, "[RENDER3D] Failed to create depth texture: %s\n", SDL_GetError());
        return false;
    }
    drawableW_ = w;
    drawableH_ = h;
    return true;
}

void RenderSystem::shutdownGPU3D() {
    SDL_GPUDevice* device = windowBackend_
        ? static_cast<SDL_GPUDevice*>(windowBackend_->getNativeGPUDevice())
        : NULL;
    if (!device) return;

    SDL_ReleaseGPUTexture(device, depthTexture_);
    depthTexture_ = NULL;
    SDL_ReleaseGPUGraphicsPipeline(device, cubePipeline_);
    cubePipeline_ = NULL;
    SDL_ReleaseGPUBuffer(device, cubeVertexBuffer_);
    cubeVertexBuffer_ = NULL;
    SDL_ReleaseGPUBuffer(device, dynamicVertexBuffer_);
    dynamicVertexBuffer_ = NULL;
    dynamicVertexCapacity_ = 0;

    gpu3DInited_ = false;
}

void RenderSystem::render3D(World* world) {
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(windowBackend_->getNativeGPUDevice());
    if (!device) return;

    if (!gpu3DInited_ && !initGPU3D()) {
        return;
    }

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        fprintf(stderr, "[RENDER3D] Failed to acquire command buffer: %s\n", SDL_GetError());
        return;
    }

    SDL_GPUTexture* swapchain = NULL;
    Uint32 drawableW, drawableH;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, static_cast<SDL_Window*>(windowBackend_->getNativeWindow()), &swapchain, &drawableW, &drawableH)) {
        fprintf(stderr, "[RENDER3D] Failed to acquire swapchain: %s\n", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }

    if (!swapchain) {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    // Recreate depth texture if window size changed
    if ((int)drawableW != drawableW_ || (int)drawableH != drawableH_) {
        if (!createDepthTexture((int)drawableW, (int)drawableH)) {
            SDL_SubmitGPUCommandBuffer(cmd);
            return;
        }
    }

    float aspect = drawableH > 0 ? (float)drawableW / (float)drawableH : 1.0f;

    // Find an active camera; fall back to a default view if none exists.
    Mat4 view;
    Mat4 projection;
    bool cameraFound = false;
    std::vector<Entity> cameras = world->queryEntitiesWith(ComponentTypeMask().withCamera().withTransform());
    for (size_t i = 0; i < cameras.size(); ++i) {
        CameraComponent* cam = world->getComponent<CameraComponent>(cameras[i]);
        if (cam && cam->isActive) {
            TransformComponent* tc = world->getComponent<TransformComponent>(cameras[i]);
            if (cam->isPerspective) {
                projection = Mat4::perspective(cam->fov, aspect, cam->nearPlane, cam->farPlane);
            } else {
                float halfH = cam->size;
                float halfW = halfH * aspect;
                projection = Mat4::ortho(-halfW, halfW, -halfH, halfH, cam->nearPlane, cam->farPlane);
            }
            view = Mat4::lookAt(tc->transform.position, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
            cameraFound = true;
            break;
        }
    }
    if (!cameraFound) {
        projection = Mat4::perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.01f, 100.0f);
        view = Mat4::lookAt(Vec3(0.0f, 8.0f, 12.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    }

    // Gather all mesh entities.
    std::vector<Entity> meshes = world->queryEntitiesWith(ComponentTypeMask().withMesh().withTransform());
    if (meshes.empty()) {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    // Resize the dynamic vertex buffer if we have more meshes than before.
    if (meshes.size() > dynamicVertexCapacity_) {
        size_t newCapacity = meshes.size() * 2;
        SDL_ReleaseGPUBuffer(device, dynamicVertexBuffer_);
        SDL_GPUBufferCreateInfo dynDesc;
        SDL_zero(dynDesc);
        dynDesc.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        dynDesc.size = sizeof(cubeVertices) * newCapacity;
        dynamicVertexBuffer_ = SDL_CreateGPUBuffer(device, &dynDesc);
        if (!dynamicVertexBuffer_) {
            fprintf(stderr, "[RENDER3D] Failed to resize dynamic vertex buffer: %s\n", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(cmd);
            return;
        }
        dynamicVertexCapacity_ = newCapacity;
    }

    // Transform cube vertices into world space for every mesh.
    std::vector<VertexData> transformed;
    transformed.reserve(meshes.size() * 36);
    for (size_t i = 0; i < meshes.size(); ++i) {
        TransformComponent* tc = world->getComponent<TransformComponent>(meshes[i]);
        MeshComponent* mc = world->getComponent<MeshComponent>(meshes[i]);
        if (!tc || !mc) continue;

        Mat4 model = tc->transform.toMatrix();
        const Color& tint = mc->color;

        for (size_t v = 0; v < sizeof(cubeVertices) / sizeof(cubeVertices[0]); ++v) {
            const VertexData& src = cubeVertices[v];
            Vec4 world = model * Vec4(src.x, src.y, src.z, 1.0f);
            VertexData dst;
            dst.x = world.x;
            dst.y = world.y;
            dst.z = world.z;
            dst.r = src.r * tint.r;
            dst.g = src.g * tint.g;
            dst.b = src.b * tint.b;
            transformed.push_back(dst);
        }
    }

    if (transformed.empty()) {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    // Upload the transformed vertices into the dynamic buffer.
    SDL_GPUTransferBufferCreateInfo transferDesc;
    SDL_zero(transferDesc);
    transferDesc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferDesc.size = transformed.size() * sizeof(VertexData);
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferDesc);
    if (transfer) {
        void* map = SDL_MapGPUTransferBuffer(device, transfer, false);
        if (map) {
            SDL_memcpy(map, transformed.data(), transferDesc.size);
            SDL_UnmapGPUTransferBuffer(device, transfer);
        }
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTransferBufferLocation loc;
        loc.transfer_buffer = transfer;
        loc.offset = 0;
        SDL_GPUBufferRegion region;
        region.buffer = dynamicVertexBuffer_;
        region.offset = 0;
        region.size = transferDesc.size;
        SDL_UploadToGPUBuffer(copyPass, &loc, &region, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
    }

    // view-projection is pushed as the MVP (model is already baked into vertices).
    Mat4 viewProj = projection * view;
    SDL_PushGPUVertexUniformData(cmd, 0, viewProj.m, sizeof(viewProj.m));

    // Render pass
    SDL_GPUColorTargetInfo colorInfo;
    SDL_zero(colorInfo);
    colorInfo.texture = swapchain;
    colorInfo.clear_color.r = 0.12f;
    colorInfo.clear_color.g = 0.12f;
    colorInfo.clear_color.b = 0.16f;
    colorInfo.clear_color.a = 1.0f;
    colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorInfo.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depthInfo;
    SDL_zero(depthInfo);
    depthInfo.texture = depthTexture_;
    depthInfo.clear_depth = 1.0f;
    depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    depthInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &colorInfo, 1, &depthInfo);
    SDL_BindGPUGraphicsPipeline(pass, cubePipeline_);

    SDL_GPUBufferBinding binding;
    binding.buffer = dynamicVertexBuffer_;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);

    SDL_DrawGPUPrimitives(pass, (Uint32)transformed.size(), 1, 0, 0);
    SDL_EndGPURenderPass(pass);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        fprintf(stderr, "[RENDER3D] Failed to submit command buffer: %s\n", SDL_GetError());
    }
}

void RenderSystem::drawSprite(const Vec3& pos, const Vec2& size, const Color& color, const char* texture) {
    (void)pos; (void)size; (void)color; (void)texture;
}

void RenderSystem::drawMesh(const Mat4& model, const char* meshPath, const char* materialPath) {
    (void)model; (void)meshPath; (void)materialPath;
}

void RenderSystem::setCamera(const Mat4& view, const Mat4& projection) {
    (void)view; (void)projection;
}

int RenderSystem::getWidth() const {
    return windowBackend_ ? windowBackend_->getWidth() : 0;
}

int RenderSystem::getHeight() const {
    return windowBackend_ ? windowBackend_->getHeight() : 0;
}

} // namespace domi
