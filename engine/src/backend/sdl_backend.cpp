#include "domi/backend/sdl_backend.h"
#include "domi/render/render_texture.h"
#include "domi/render/material.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace domi {

struct MaterialCache {
    // Textures are deduplicated by content hash, so several callers can hold
    // the same texture (e.g. the scene and an editor preview of an identical
    // material). Refcounted: only the last destroyMaterial() actually
    // destroys the texture.
    struct Entry {
        SDL_Texture* texture;
        int refs;
    };
    std::unordered_map<uint64_t, Entry> textures;
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
    : window_(NULL), renderer_(NULL), whiteTexture_(NULL), gpuDevice_(NULL),
      target3D_(NULL), gpuClaimed_(false), width_(0), height_(0), currentTarget_(NULL),
      mouseX_(0), mouseY_(0), mouseDeltaX_(0), mouseDeltaY_(0),
      scrollX_(0), scrollY_(0), inputInitialized_(false), audioInitialized_(false),
      materialCache_(new MaterialCache()),
      lockedPixels3D_(NULL), lockedPitch3D_(0), in3D_(false), pairId3D_(0),
      present3DX0_(0), present3DY0_(0), present3DX1_(0), present3DY1_(0),
      drawable3DW_(0), drawable3DH_(0) {
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

    int numDrivers = SDL_GetNumRenderDrivers();
    fprintf(stderr, "[SDLBackend] Available render drivers: %d\n", numDrivers);
    for (int i = 0; i < numDrivers; ++i) {
        fprintf(stderr, "[SDLBackend]   driver %d: %s\n", i, SDL_GetRenderDriver(i));
    }

    // Try the Vulkan SDL_Renderer backend first. Note: this only changes how
    // the final 2D output is presented; the voxel 3D path still rasterizes on
    // the CPU into a locked texture before handing it to SDL.
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");

    renderer_ = SDL_CreateRenderer(window_, NULL);
    if (!renderer_) {
        fprintf(stderr,
                "[SDLBackend] Vulkan renderer unavailable (%s), falling back\n",
                SDL_GetError());
        SDL_ResetHint(SDL_HINT_RENDER_DRIVER);
        renderer_ = SDL_CreateRenderer(window_, NULL);
    }
    if (!renderer_) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = NULL;
        return false;
    }

    const char* rendererName = SDL_GetRendererName(renderer_);
    fprintf(stderr, "[SDLBackend] Selected renderer: %s\n",
            rendererName ? rendererName : "unknown");

    // Create a 1x1 white texture for color-only geometry draws.
    whiteTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (whiteTexture_) {
        uint32_t white = 0xFFFFFFFF;
        SDL_UpdateTexture(whiteTexture_, NULL, &white, 4);
        SDL_SetTextureBlendMode(whiteTexture_, SDL_BLENDMODE_BLEND);
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
            if (it.second.texture) SDL_DestroyTexture(it.second.texture);
        }
        materialCache_->textures.clear();
    }

    for (size_t i = 0; i < mutableTextures_.size(); ++i) {
        if (mutableTextures_[i]) SDL_DestroyTexture(mutableTextures_[i]);
    }
    mutableTextures_.clear();

    if (whiteTexture_) {
        SDL_DestroyTexture(whiteTexture_);
        whiteTexture_ = NULL;
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
                             BlendMode mode, float angle,
                             float centerX, float centerY,
                             float scaleX, float scaleY) {
    if (!renderer_ || !texture || !texture->valid()) return;

    SDL_Texture* native = static_cast<SDL_Texture*>(texture->getNative());
    SDL_BlendMode oldMode;
    SDL_GetTextureBlendMode(native, &oldMode);
    SDL_BlendMode newMode = toSDLBlendMode(mode);
    if (oldMode != newMode) {
        SDL_SetTextureBlendMode(native, newMode);
    }

    float w = (float)texture->width() * scaleX;
    float h = (float)texture->height() * scaleY;
    SDL_FRect dst = { x, y, w, h };
    SDL_FPoint center = { centerX, centerY };
    SDL_RenderTextureRotated(renderer_, native, NULL, &dst, angle, &center,
                             SDL_FLIP_NONE);

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
        ++it->second.refs;
        return it->second.texture;
    }

    SDL_Texture* tex = uploadMaterialTexture(renderer_, material);
    if (!tex) return NULL;

    MaterialCache::Entry entry;
    entry.texture = tex;
    entry.refs = 1;
    materialCache_->textures[hash] = entry;
    return tex;
}

void SDLBackend::drawMaterial(float x, float y, void* handle,
                              float angle, float centerX, float centerY,
                              float scaleX, float scaleY) {
    if (!renderer_ || !handle) return;

    SDL_Texture* tex = static_cast<SDL_Texture*>(handle);
    float w, h;
    if (!SDL_GetTextureSize(tex, &w, &h)) return;

    w *= scaleX;
    h *= scaleY;
    SDL_FRect dst = { x, y, w, h };
    SDL_FPoint center = { centerX, centerY };
    SDL_RenderTextureRotated(renderer_, tex, NULL, &dst, angle, &center,
                             SDL_FLIP_NONE);
}

void SDLBackend::destroyMaterial(void* handle) {
    if (!handle) return;

    SDL_Texture* tex = static_cast<SDL_Texture*>(handle);

    // Mutable textures are not in the content-hash cache.
    for (std::vector<SDL_Texture*>::iterator it = mutableTextures_.begin();
         it != mutableTextures_.end(); ++it) {
        if (*it == tex) {
            SDL_DestroyTexture(tex);
            mutableTextures_.erase(it);
            return;
        }
    }

    if (!materialCache_) return;
    for (auto it = materialCache_->textures.begin();
         it != materialCache_->textures.end(); ++it) {
        if (it->second.texture == tex) {
            // Shared texture: only destroy once the last reference is gone.
            if (--it->second.refs <= 0) {
                SDL_DestroyTexture(it->second.texture);
                materialCache_->textures.erase(it);
            }
            return;
        }
    }
}

void* SDLBackend::createMutableTexture(int width, int height) {
    if (!renderer_ || width <= 0 || height <= 0) return NULL;

    SDL_Texture* tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         width, height);
    if (!tex) return NULL;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    // Start fully transparent so undrawn regions never sample garbage.
    std::vector<uint8_t> zeros((size_t)width * height * 4, 0);
    SDL_UpdateTexture(tex, NULL, zeros.data(), width * 4);

    mutableTextures_.push_back(tex);
    return tex;
}

void SDLBackend::updateTextureRegion(void* handle, int x, int y, int w, int h,
                                     const void* rgbaPixels) {
    if (!renderer_ || !handle || !rgbaPixels || w <= 0 || h <= 0) return;

    SDL_Rect rect = { x, y, w, h };
    SDL_UpdateTexture(static_cast<SDL_Texture*>(handle), &rect,
                      rgbaPixels, w * 4);
}

void SDLBackend::drawMaterialRegion(float x, float y, void* handle,
                                    int srcX, int srcY, int srcW, int srcH,
                                    const Color& tint,
                                    float angle, float centerX, float centerY,
                                    float scaleX, float scaleY) {
    if (!renderer_ || !handle || srcW <= 0 || srcH <= 0) return;

    SDL_Texture* tex = static_cast<SDL_Texture*>(handle);

    SDL_FRect src = { (float)srcX, (float)srcY, (float)srcW, (float)srcH };
    SDL_FRect dst = { x, y, srcW * scaleX, srcH * scaleY };
    SDL_FPoint center = { centerX, centerY };

    uint8_t tr = (uint8_t)(tint.r * 255.0f);
    uint8_t tg = (uint8_t)(tint.g * 255.0f);
    uint8_t tb = (uint8_t)(tint.b * 255.0f);
    uint8_t ta = (uint8_t)(tint.a * 255.0f);
    SDL_SetTextureColorMod(tex, tr, tg, tb);
    SDL_SetTextureAlphaMod(tex, ta);

    SDL_RenderTextureRotated(renderer_, tex, &src, &dst, angle, &center,
                             SDL_FLIP_NONE);

    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, 255);
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

void SDLBackend::resize3DBuffers(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (w == drawable3DW_ && h == drawable3DH_) return;
    depthBuffer3D_.assign((size_t)w * h, 1.0f);
    pairStamp3D_.assign((size_t)w * h, -1);
    drawable3DW_ = w;
    drawable3DH_ = h;
}

void SDLBackend::begin3D() {
    if (!renderer_ || in3D_) return;
    ensure3DTarget();
    if (!target3D_) return;

    if (SDL_LockTexture(target3D_, NULL, &lockedPixels3D_, &lockedPitch3D_)) {
        in3D_ = true;
        ++pairId3D_;
        if (pairId3D_ < 0) {
            std::fill(pairStamp3D_.begin(), pairStamp3D_.end(), -1);
            pairId3D_ = 0;
        }
        present3DX0_ = present3DY0_ = present3DX1_ = present3DY1_ = 0;

        int w, h;
        SDL_GetRenderOutputSize(renderer_, &w, &h);
        resize3DBuffers(w, h);
    }
}

void SDLBackend::end3D() {
    if (!in3D_) return;
    in3D_ = false;

    if (lockedPixels3D_ && present3DX1_ > present3DX0_ && present3DY1_ > present3DY0_) {
        for (int y = present3DY0_; y < present3DY1_; ++y) {
            uint8_t* row = (uint8_t*)lockedPixels3D_ + y * lockedPitch3D_;
            int idx = y * drawable3DW_ + present3DX0_;
            for (int x = present3DX0_; x < present3DX1_; ++x, ++idx) {
                if (pairStamp3D_[idx] != pairId3D_) {
                    *(uint32_t*)(row + (size_t)x * 4) = 0;
                }
            }
        }
    }

    if (target3D_) {
        SDL_UnlockTexture(target3D_);
    }
    lockedPixels3D_ = NULL;

    int w = present3DX1_ - present3DX0_;
    int h = present3DY1_ - present3DY0_;
    if (target3D_ && w > 0 && h > 0) {
        SDL_FRect r = { (float)present3DX0_, (float)present3DY0_, (float)w, (float)h };
        SDL_RenderTexture(renderer_, target3D_, &r, &r);
    }
}

void SDLBackend::rasterizeTriangle(const Vec2& a, const Vec2& b, const Vec2& c,
                                   float za, float zb, float zc,
                                   const Color& color) {
    if (!in3D_ || !lockedPixels3D_ || drawable3DW_ <= 0 || drawable3DH_ <= 0) return;

    float minX = std::min(std::min(a.x, b.x), c.x);
    float minY = std::min(std::min(a.y, b.y), c.y);
    float maxX = std::max(std::max(a.x, b.x), c.x);
    float maxY = std::max(std::max(a.y, b.y), c.y);

    if (maxX < 0.0f || maxY < 0.0f || minX >= drawable3DW_ || minY >= drawable3DH_) return;
    if (minX < 0.0f) minX = 0.0f;
    if (minY < 0.0f) minY = 0.0f;
    if (maxX >= drawable3DW_) maxX = drawable3DW_ - 1;
    if (maxY >= drawable3DH_) maxY = drawable3DH_ - 1;

    const int bx0 = (int)minX, by0 = (int)minY;
    const int bx1 = (int)maxX + 1, by1 = (int)maxY + 1;
    if (present3DX1_ <= present3DX0_ || present3DY1_ <= present3DY0_) {
        present3DX0_ = bx0; present3DY0_ = by0;
        present3DX1_ = bx1; present3DY1_ = by1;
    } else {
        if (bx0 < present3DX0_) present3DX0_ = bx0;
        if (by0 < present3DY0_) present3DY0_ = by0;
        if (bx1 > present3DX1_) present3DX1_ = bx1;
        if (by1 > present3DY1_) present3DY1_ = by1;
    }

    Vec3 v0(c.x - a.x, c.y - a.y, 0.0f);
    Vec3 v1(b.x - a.x, b.y - a.y, 0.0f);
    float dot00 = Vec3::dot(v0, v0);
    float dot01 = Vec3::dot(v0, v1);
    float dot11 = Vec3::dot(v1, v1);
    float denom = dot00 * dot11 - dot01 * dot01;
    if (denom == 0.0f) return;

    uint8_t cr = (uint8_t)(color.r * 255.0f);
    uint8_t cg = (uint8_t)(color.g * 255.0f);
    uint8_t cb = (uint8_t)(color.b * 255.0f);
    uint8_t ca = (uint8_t)(color.a * 255.0f);

    for (int y = (int)minY; y <= (int)maxY; ++y) {
        uint8_t* row = (uint8_t*)lockedPixels3D_ + y * lockedPitch3D_;
        for (int x = (int)minX; x <= (int)maxX; ++x) {
            Vec3 v2((float)x - a.x, (float)y - a.y, 0.0f);
            float dot20 = Vec3::dot(v0, v2);
            float dot21 = Vec3::dot(v1, v2);
            float gamma = (dot11 * dot20 - dot01 * dot21) / denom;
            float beta  = (dot00 * dot21 - dot01 * dot20) / denom;
            float alpha = 1.0f - gamma - beta;
            if (alpha < 0.0f || beta < 0.0f || gamma < 0.0f) continue;

            float z = alpha * za + beta * zb + gamma * zc;
            int idx = y * drawable3DW_ + x;
            if (pairStamp3D_[idx] != pairId3D_ || z < depthBuffer3D_[idx]) {
                pairStamp3D_[idx] = pairId3D_;
                depthBuffer3D_[idx] = z;
                int px = x * 4;
                row[px + 0] = cr;
                row[px + 1] = cg;
                row[px + 2] = cb;
                row[px + 3] = ca;
            }
        }
    }
}

void SDLBackend::fillTriangle3D(const Vec2& a, const Vec2& b, const Vec2& c,
                                float za, float zb, float zc,
                                const Color& color) {
    rasterizeTriangle(a, b, c, za, zb, zc, color);
}

void SDLBackend::fillAffineRect3D(const Vec2& origin, const Vec2& size,
                                  const Affine2D& transform, float z,
                                  const Color& color) {
    (void)z;
    if (!renderer_) return;

    // GPU path: draw the affine-mapped rectangle directly via SDL_RenderGeometry.
    // The caller is responsible for depth ordering (e.g. painter's algorithm);
    // this SDL backend does not perform per-pixel depth testing here.
    Vec2 p0 = transform * Vec2(origin.x, origin.y);
    Vec2 p1 = transform * Vec2(origin.x + size.x, origin.y);
    Vec2 p2 = transform * Vec2(origin.x + size.x, origin.y + size.y);
    Vec2 p3 = transform * Vec2(origin.x, origin.y + size.y);

    SDL_FColor col = toSDLColor(color);
    SDL_FPoint zero = { 0.0f, 0.0f };

    SDL_Vertex verts[4];
    verts[0].position = { p0.x, p0.y };
    verts[0].color    = col;
    verts[0].tex_coord = zero;
    verts[1].position = { p1.x, p1.y };
    verts[1].color    = col;
    verts[1].tex_coord = zero;
    verts[2].position = { p2.x, p2.y };
    verts[2].color    = col;
    verts[2].tex_coord = zero;
    verts[3].position = { p3.x, p3.y };
    verts[3].color    = col;
    verts[3].tex_coord = zero;

    // SDL_RenderGeometry multiplies vertex colors by the renderer's current
    // draw color, so make sure the modulation is white/opaque.
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);

    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_Texture* tex = whiteTexture_ ? whiteTexture_ : NULL;
    if (!SDL_RenderGeometry(renderer_, tex, verts, 4, indices, 6)) {
        fprintf(stderr, "[FILLAFFINE] SDL_RenderGeometry failed: %s\n", SDL_GetError());
    }
}

void SDLBackend::fillQuad3D(const Vec2& p0, const Vec2& p1, const Vec2& p2,
                            const Vec2& p3, float z, const Color& color) {
    (void)z;
    if (!renderer_) return;

    // GPU path: draw the general quadrilateral as two triangles via
    // SDL_RenderGeometry. The caller is responsible for depth ordering.
    SDL_FColor col = toSDLColor(color);
    SDL_FPoint zero = { 0.0f, 0.0f };

    SDL_Vertex verts[4];
    verts[0].position = { p0.x, p0.y };
    verts[0].color    = col;
    verts[0].tex_coord = zero;
    verts[1].position = { p1.x, p1.y };
    verts[1].color    = col;
    verts[1].tex_coord = zero;
    verts[2].position = { p2.x, p2.y };
    verts[2].color    = col;
    verts[2].tex_coord = zero;
    verts[3].position = { p3.x, p3.y };
    verts[3].color    = col;
    verts[3].tex_coord = zero;

    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);

    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_Texture* tex = whiteTexture_ ? whiteTexture_ : NULL;
    if (!SDL_RenderGeometry(renderer_, tex, verts, 4, indices, 6)) {
        fprintf(stderr, "[FILLQUAD] SDL_RenderGeometry failed: %s\n", SDL_GetError());
    }
}

} // namespace domi
