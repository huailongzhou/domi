#include "domi/backend/sdl_backend.h"
#include "domi/render_texture.h"
#include "domi/material.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace domi {

struct MaterialCache {
    std::unordered_map<uint64_t, SDL_Texture*> textures;
};

namespace {

SDL_FColor toSDLColor(const Color& c) {
    SDL_FColor fc;
    fc.r = c.r;
    fc.g = c.g;
    fc.b = c.b;
    fc.a = c.a;
    return fc;
}

SDL_BlendMode toSDLBlendMode(BlendMode mode) {
    switch (mode) {
        case BlendMode::Blend: return SDL_BLENDMODE_BLEND;
        case BlendMode::Add:   return SDL_BLENDMODE_ADD;
        case BlendMode::Mod:   return SDL_BLENDMODE_MOD;
        default:               return SDL_BLENDMODE_NONE;
    }
}

SDL_PixelFormat toSDLRenderTextureFormat(RenderTextureFormat format) {
    switch (format) {
        default:
        case RenderTextureFormat::RGBA8888: return SDL_PIXELFORMAT_RGBA8888;
    }
}

void renderGeometryPath(SDL_Renderer* renderer, const std::vector<Vec2>& path,
                        bool closed, const Color& color) {
    size_t n = path.size();
    if (n < 3) return;

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(n);
    std::vector<int> indices;
    indices.reserve((n - 2) * 3);

    SDL_FColor col = toSDLColor(color);
    SDL_FPoint zero = { 0.0f, 0.0f };

    for (size_t i = 0; i < n; ++i) {
        SDL_Vertex v;
        v.position.x = path[i].x;
        v.position.y = path[i].y;
        v.color = col;
        v.tex_coord = zero;
        vertices.push_back(v);
    }

    for (size_t i = 1; i + 1 < n; ++i) {
        indices.push_back(0);
        indices.push_back((int)i);
        indices.push_back((int)(i + 1));
    }

    SDL_RenderGeometry(renderer, NULL, vertices.data(), (int)vertices.size(),
                       indices.data(), (int)indices.size());
}

void strokeGeometryPath(SDL_Renderer* renderer, const std::vector<Vec2>& path,
                        bool closed, const Color& color) {
    size_t n = path.size();
    if (n < 2) return;

    SDL_SetRenderDrawColor(renderer,
        (uint8_t)(color.r * 255),
        (uint8_t)(color.g * 255),
        (uint8_t)(color.b * 255),
        (uint8_t)(color.a * 255));

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        SDL_RenderLine(renderer, path[i].x, path[i].y,
                       path[i + 1].x, path[i + 1].y);
    }

    if (closed && path.size() > 2) {
        SDL_RenderLine(renderer, path.back().x, path.back().y,
                       path[0].x, path[0].y);
    }
}

// Convert LUT8 / AlphaMask materials to a CPU-side RGBA buffer that SDL can
// upload directly. ARGB8888 and RGB565 are uploaded with their native SDL
// pixel formats instead to avoid channel-order issues.
std::vector<uint8_t> materialToRGBA(const domi::Material& mat) {
    const int count = mat.width * mat.height;
    std::vector<uint8_t> rgba(count * 4, 0);

    switch (mat.format) {
        case domi::PixelFormat::LUT8: {
            for (int i = 0; i < count; ++i) {
                uint8_t idx = mat.pixels[i];
                const domi::Color& c = idx < mat.palette.size()
                                           ? mat.palette[idx]
                                           : domi::Color(0, 0, 0, 1);
                rgba[i * 4 + 0] = (uint8_t)(c.r * 255.0f);
                rgba[i * 4 + 1] = (uint8_t)(c.g * 255.0f);
                rgba[i * 4 + 2] = (uint8_t)(c.b * 255.0f);
                rgba[i * 4 + 3] = (uint8_t)(c.a * 255.0f);
            }
            break;
        }

        case domi::PixelFormat::AlphaMask: {
            for (int i = 0; i < count; ++i) {
                uint8_t a = mat.pixels[i];
                rgba[i * 4 + 0] = 255;
                rgba[i * 4 + 1] = 255;
                rgba[i * 4 + 2] = 255;
                rgba[i * 4 + 3] = a;
            }
            break;
        }

        default:
            break;
    }

    return rgba;
}

// Simple FNV-1a hash over a byte range.
uint64_t hashBytes(const uint8_t* data, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

uint64_t computeMaterialHash(const domi::Material& mat) {
    uint64_t h = hashBytes((const uint8_t*)&mat.width, sizeof(mat.width));
    h ^= hashBytes((const uint8_t*)&mat.height, sizeof(mat.height)) + 0x9e3779b97f4a7c15ULL;
    int fmt = static_cast<int>(mat.format);
    h ^= hashBytes((const uint8_t*)&fmt, sizeof(fmt)) + 0x9e3779b97f4a7c15ULL;
    h ^= hashBytes(mat.pixels.data(), mat.pixels.size());
    if (mat.format == domi::PixelFormat::LUT8) {
        for (size_t i = 0; i < mat.palette.size(); ++i) {
            const domi::Color& c = mat.palette[i];
            float vals[4] = { c.r, c.g, c.b, c.a };
            h ^= hashBytes((const uint8_t*)vals, sizeof(vals)) + i;
        }
    }
    return h;
}

SDL_Texture* uploadMaterialTexture(SDL_Renderer* renderer,
                                   const domi::Material& material) {
    if (!renderer || material.width <= 0 || material.height <= 0) return NULL;

    SDL_PixelFormat sdlFormat = SDL_PIXELFORMAT_RGBA8888;
    int pixelBytes = 4;
    const uint8_t* srcPixels = NULL;
    std::vector<uint8_t> converted;

    switch (material.format) {
        case domi::PixelFormat::ARGB8888:
            sdlFormat = SDL_PIXELFORMAT_ARGB32;
            pixelBytes = 4;
            srcPixels = material.pixels.data();
            break;

        case domi::PixelFormat::RGB565:
            sdlFormat = SDL_PIXELFORMAT_RGB565;
            pixelBytes = 2;
            srcPixels = material.pixels.data();
            break;

        case domi::PixelFormat::LUT8:
        case domi::PixelFormat::AlphaMask:
            sdlFormat = SDL_PIXELFORMAT_RGBA32;
            pixelBytes = 4;
            converted = materialToRGBA(material);
            srcPixels = converted.data();
            break;
    }

    SDL_Texture* tex = SDL_CreateTexture(renderer, sdlFormat,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         material.width, material.height);
    if (!tex) return NULL;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    void* pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch)) {
        int srcPitch = material.width * pixelBytes;
        if (pitch == srcPitch) {
            memcpy(pixels, srcPixels, material.height * srcPitch);
        } else {
            for (int row = 0; row < material.height; ++row) {
                memcpy((uint8_t*)pixels + row * pitch,
                       srcPixels + row * srcPitch,
                       srcPitch);
            }
        }
        SDL_UnlockTexture(tex);
    }

    return tex;
}

} // anonymous namespace

SDLBackend::SDLBackend()
    : window_(NULL), renderer_(NULL), gpuDevice_(NULL), target3D_(NULL),
      gpuClaimed_(false), width_(0), height_(0), currentTarget_(NULL),
      mouseX_(0), mouseY_(0), mouseDeltaX_(0), mouseDeltaY_(0),
      scrollX_(0), scrollY_(0), inputInitialized_(false), audioInitialized_(false),
      materialCache_(new MaterialCache()) {
    memset(keysCurr_, 0, sizeof(keysCurr_));
    memset(keysPrev_, 0, sizeof(keysPrev_));
    memset(mouseCurr_, 0, sizeof(mouseCurr_));
    memset(mousePrev_, 0, sizeof(mousePrev_));
}

SDLBackend::~SDLBackend() {
    destroy();
}

bool SDLBackend::initializePlatform() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void SDLBackend::shutdownPlatform() {
    SDL_Quit();
}

// -----------------------------------------------------------------
// IInputBackend
// -----------------------------------------------------------------

bool SDLBackend::init() {
    if (!inputInitialized_) {
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            fprintf(stderr, "SDL gamepad init failed: %s\n", SDL_GetError());
            // Non-fatal: keyboard/mouse still work without gamepad.
        }
        inputInitialized_ = true;
    }
    if (!audioInitialized_) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
            return false;
        }
        audioInitialized_ = true;
    }
    return true;
}

void SDLBackend::shutdown() {
    if (inputInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        inputInitialized_ = false;
    }
    if (audioInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioInitialized_ = false;
    }
}

void SDLBackend::update() {
    memcpy(keysPrev_, keysCurr_, sizeof(keysCurr_));
    memcpy(mousePrev_, mouseCurr_, sizeof(mouseCurr_));
    mouseDeltaX_ = 0;
    mouseDeltaY_ = 0;
    scrollX_ = 0;
    scrollY_ = 0;
}

void SDLBackend::handleEvent(const void* nativeEvent) {
    const SDL_Event* e = static_cast<const SDL_Event*>(nativeEvent);
    if (!e) return;
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN:
        if (e->key.scancode < MAX_KEYS) keysCurr_[e->key.scancode] = true;
        break;
    case SDL_EVENT_KEY_UP:
        if (e->key.scancode < MAX_KEYS) keysCurr_[e->key.scancode] = false;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e->button.button < MAX_MOUSE) mouseCurr_[e->button.button] = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e->button.button < MAX_MOUSE) mouseCurr_[e->button.button] = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        mouseDeltaX_ = e->motion.xrel;
        mouseDeltaY_ = e->motion.yrel;
        mouseX_ = e->motion.x;
        mouseY_ = e->motion.y;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        scrollX_ = e->wheel.x;
        scrollY_ = e->wheel.y;
        break;
    }
}

bool SDLBackend::isKeyDown(int key) const {
    return key >= 0 && key < MAX_KEYS && keysCurr_[key];
}

bool SDLBackend::isKeyPressed(int key) const {
    return key >= 0 && key < MAX_KEYS && keysCurr_[key] && !keysPrev_[key];
}

bool SDLBackend::isKeyReleased(int key) const {
    return key >= 0 && key < MAX_KEYS && !keysCurr_[key] && keysPrev_[key];
}

bool SDLBackend::isMouseButtonDown(int button) const {
    return button >= 0 && button < MAX_MOUSE && mouseCurr_[button];
}

bool SDLBackend::isMouseButtonPressed(int button) const {
    return button >= 0 && button < MAX_MOUSE && mouseCurr_[button] && !mousePrev_[button];
}

float SDLBackend::getAxis(const char* name) const {
    if (strcmp(name, "Horizontal") == 0) {
        float v = 0;
        if (isKeyDown(SDL_SCANCODE_D) || isKeyDown(SDL_SCANCODE_RIGHT)) v += 1;
        if (isKeyDown(SDL_SCANCODE_A) || isKeyDown(SDL_SCANCODE_LEFT)) v -= 1;
        return v;
    }
    if (strcmp(name, "Vertical") == 0) {
        float v = 0;
        if (isKeyDown(SDL_SCANCODE_W) || isKeyDown(SDL_SCANCODE_UP)) v -= 1;
        if (isKeyDown(SDL_SCANCODE_S) || isKeyDown(SDL_SCANCODE_DOWN)) v += 1;
        return v;
    }
    return 0;
}

// -----------------------------------------------------------------
// IAudioBackend
// -----------------------------------------------------------------

void SDLBackend::play(const char* path, bool loop) {
    (void)path; (void)loop;
    // TODO: implement audio playback
}

void SDLBackend::stop(const char* path) {
    (void)path;
}

void SDLBackend::stopAll() {}

// -----------------------------------------------------------------
// IWindowBackend
// -----------------------------------------------------------------

bool SDLBackend::create(const std::string& title, int width, int height) {
    width_ = width;
    height_ = height;

    window_ = SDL_CreateWindow(title.c_str(), width, height, 0);
    if (!window_) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, NULL);
    if (!renderer_) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = NULL;
        return false;
    }

    // Use alpha blending for all renderer draw operations so that translucent
    // fills and overlays work correctly.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    if (!SDL_SetRenderVSync(renderer_, 1)) {
        fprintf(stderr, "Failed to enable VSync: %s\n", SDL_GetError());
    }

    SDL_RaiseWindow(window_);
    SDL_SetWindowKeyboardGrab(window_, true);

    gpuDevice_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
    if (!gpuDevice_) {
        fprintf(stderr, "GPU device creation failed, 3D will be unavailable\n");
    }

    return true;
}

void SDLBackend::destroy() {
    if (materialCache_) {
        for (auto& it : materialCache_->textures) {
            if (it.second) SDL_DestroyTexture(it.second);
        }
        materialCache_->textures.clear();
    }

    if (target3D_) {
        SDL_DestroyTexture(target3D_);
        target3D_ = NULL;
    }
    if (gpuDevice_) {
        if (gpuClaimed_) {
            SDL_ReleaseWindowFromGPUDevice(gpuDevice_, window_);
            gpuClaimed_ = false;
        }
        SDL_DestroyGPUDevice(gpuDevice_);
        gpuDevice_ = NULL;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = NULL;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = NULL;
    }
}

bool SDLBackend::claimGPU() {
    if (!gpuDevice_ || gpuClaimed_) return gpuClaimed_;
    if (!SDL_ClaimWindowForGPUDevice(gpuDevice_, window_)) {
        fprintf(stderr, "Failed to claim window for GPU: %s\n", SDL_GetError());
        return false;
    }
    gpuClaimed_ = true;
    return true;
}

void SDLBackend::releaseGPU() {
    if (gpuDevice_ && gpuClaimed_) {
        SDL_ReleaseWindowFromGPUDevice(gpuDevice_, window_);
        gpuClaimed_ = false;
    }
}

// -----------------------------------------------------------------
// IRenderBackend
// -----------------------------------------------------------------

void SDLBackend::setTarget(RenderTexture* target) {
    if (!renderer_ || target == currentTarget_) return;

    SDL_Texture* native = NULL;
    if (target) {
        native = static_cast<SDL_Texture*>(target->getNative());
    }
    SDL_SetRenderTarget(renderer_, native);
    currentTarget_ = target;
}

void SDLBackend::clear(const Color& c) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_,
        (uint8_t)(c.r * 255),
        (uint8_t)(c.g * 255),
        (uint8_t)(c.b * 255),
        (uint8_t)(c.a * 255));
    SDL_RenderClear(renderer_);
}

void SDLBackend::setClipRect(float x, float y, float w, float h) {
    if (!renderer_) return;
    SDL_Rect rect = { (int)x, (int)y, (int)w, (int)h };
    SDL_SetRenderClipRect(renderer_, &rect);
}

void SDLBackend::resetClipRect() {
    if (!renderer_) return;
    SDL_SetRenderClipRect(renderer_, NULL);
}

void SDLBackend::present() {
    if (renderer_) {
        SDL_RenderPresent(renderer_);
    }
}

void SDLBackend::setFillColor(const Color& c) {
    currentFillColor_ = c;
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_,
        (uint8_t)(c.r * 255),
        (uint8_t)(c.g * 255),
        (uint8_t)(c.b * 255),
        (uint8_t)(c.a * 255));
}

void SDLBackend::setStrokeColor(const Color& c) {
    currentStrokeColor_ = c;
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_,
        (uint8_t)(c.r * 255),
        (uint8_t)(c.g * 255),
        (uint8_t)(c.b * 255),
        (uint8_t)(c.a * 255));
}

void SDLBackend::setLineWidth(float w) {
    if (!renderer_) return;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
    // SDL3 does not expose a line-width API; stroke width is emulated by scale
    // in immediate mode callers if needed. For now we accept the parameter but
    // cannot change the physical width.
    (void)w;
}

void SDLBackend::fillRect(float x, float y, float w, float h) {
    if (!renderer_) return;
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer_, &rect);
}

void SDLBackend::strokeRect(float x, float y, float w, float h) {
    if (!renderer_) return;
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderRect(renderer_, &rect);
}

void SDLBackend::drawLine(float x1, float y1, float x2, float y2) {
    if (!renderer_) return;
    SDL_RenderLine(renderer_, x1, y1, x2, y2);
}

void SDLBackend::fillCircle(float x, float y, float radius, int segments) {
    if (!renderer_ || segments < 3) return;

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(segments + 2);
    std::vector<int> indices;
    indices.reserve(segments * 3);

    SDL_FColor col = toSDLColor(currentFillColor_);
    SDL_FPoint zero = { 0.0f, 0.0f };

    SDL_Vertex center;
    center.position.x = x;
    center.position.y = y;
    center.color = col;
    center.tex_coord = zero;
    vertices.push_back(center);

    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * 3.14159265f;
        SDL_Vertex v;
        v.position.x = x + radius * std::cos(angle);
        v.position.y = y + radius * std::sin(angle);
        v.color = col;
        v.tex_coord = zero;
        vertices.push_back(v);
    }

    for (int i = 0; i < segments; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }

    SDL_RenderGeometry(renderer_, NULL, vertices.data(), (int)vertices.size(),
                       indices.data(), (int)indices.size());
}

void SDLBackend::fillPath(const std::vector<Vec2>& points, bool closed,
                          const Color& color) {
    if (!renderer_) return;
    renderGeometryPath(renderer_, points, closed, color);
}

void SDLBackend::strokePath(const std::vector<Vec2>& points, bool closed,
                            const Color& color, float lineWidth) {
    if (!renderer_) return;
    (void)lineWidth; // SDL3 does not support variable line width.
    strokeGeometryPath(renderer_, points, closed, color);
}

void SDLBackend::drawTexture(float x, float y, RenderTexture* texture,
                             BlendMode mode) {
    if (!renderer_ || !texture || !texture->valid()) return;

    SDL_Texture* native = static_cast<SDL_Texture*>(texture->getNative());
    SDL_BlendMode oldMode;
    SDL_GetTextureBlendMode(native, &oldMode);
    SDL_BlendMode newMode = toSDLBlendMode(mode);
    if (oldMode != newMode) {
        SDL_SetTextureBlendMode(native, newMode);
    }

    SDL_FRect dst = { x, y, (float)texture->width(), (float)texture->height() };
    SDL_RenderTexture(renderer_, native, NULL, &dst);

    if (oldMode != newMode) {
        SDL_SetTextureBlendMode(native, oldMode);
    }
}

void* SDLBackend::uploadMaterial(const Material& material) {
    if (!renderer_ || !materialCache_) return NULL;
    if (material.width <= 0 || material.height <= 0) return NULL;

    uint64_t hash = computeMaterialHash(material);
    auto it = materialCache_->textures.find(hash);
    if (it != materialCache_->textures.end()) {
        return it->second;
    }

    SDL_Texture* tex = uploadMaterialTexture(renderer_, material);
    if (!tex) return NULL;

    materialCache_->textures[hash] = tex;
    return tex;
}

void SDLBackend::drawMaterial(float x, float y, void* handle) {
    if (!renderer_ || !handle) return;

    SDL_Texture* tex = static_cast<SDL_Texture*>(handle);
    float w, h;
    if (!SDL_GetTextureSize(tex, &w, &h)) return;

    SDL_FRect dst = { x, y, w, h };
    SDL_RenderTexture(renderer_, tex, NULL, &dst);
}

void SDLBackend::destroyMaterial(void* handle) {
    if (!materialCache_ || !handle) return;

    SDL_Texture* tex = static_cast<SDL_Texture*>(handle);
    for (auto it = materialCache_->textures.begin();
         it != materialCache_->textures.end(); ++it) {
        if (it->second == tex) {
            SDL_DestroyTexture(it->second);
            materialCache_->textures.erase(it);
            return;
        }
    }
}

void* SDLBackend::createRenderTarget(int width, int height,
                                     RenderTextureFormat format) {
    if (!renderer_ || width <= 0 || height <= 0) return NULL;

    SDL_Texture* tex = SDL_CreateTexture(renderer_, toSDLRenderTextureFormat(format),
                                         SDL_TEXTUREACCESS_TARGET, width, height);
    if (!tex) {
        fprintf(stderr, "[SDLBackend] Failed to create render target\n");
        return NULL;
    }

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

void SDLBackend::destroyRenderTarget(void* handle) {
    if (handle) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(handle));
    }
}

void SDLBackend::ensure3DTarget() {
    if (!renderer_ || target3D_) return;

    int w, h;
    SDL_GetRenderOutputSize(renderer_, &w, &h);
    target3D_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                  SDL_TEXTUREACCESS_STREAMING, w, h);
    if (target3D_) {
        SDL_SetTextureBlendMode(target3D_, SDL_BLENDMODE_BLEND);
    }
}

bool SDLBackend::lock3DTarget(void** pixels, int* pitch) {
    if (!renderer_ || !pixels || !pitch) return false;
    ensure3DTarget();
    if (!target3D_) return false;
    if (SDL_LockTexture(target3D_, NULL, pixels, pitch)) {
        // Clear to transparent.
        int w, h;
        SDL_GetRenderOutputSize(renderer_, &w, &h);
        memset(*pixels, 0, (*pitch) * h);
        return true;
    }
    return false;
}

void SDLBackend::unlock3DTarget() {
    if (target3D_) {
        SDL_UnlockTexture(target3D_);
    }
}

void SDLBackend::present3DTarget() {
    if (target3D_) {
        SDL_RenderTexture(renderer_, target3D_, NULL, NULL);
    }
}

} // namespace domi
