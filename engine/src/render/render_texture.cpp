#include "domi/render_texture.h"

namespace domi {

RenderTexture::RenderTexture()
    : backend_(NULL), handle_(NULL), width_(0), height_(0),
      format_(RenderTextureFormat::RGBA8888) {}

RenderTexture::~RenderTexture() {
    destroy();
}

bool RenderTexture::create(IRenderBackend* backend, int width, int height,
                           RenderTextureFormat format) {
    if (!backend || width <= 0 || height <= 0) return false;

    destroy();

    void* handle = backend->createRenderTarget(width, height, format);
    if (!handle) return false;

    backend_ = backend;
    handle_ = handle;
    width_ = width;
    height_ = height;
    format_ = format;
    return true;
}

void RenderTexture::destroy() {
    if (backend_ && handle_) {
        backend_->destroyRenderTarget(handle_);
    }
    backend_ = NULL;
    handle_ = NULL;
    width_ = 0;
    height_ = 0;
    format_ = RenderTextureFormat::RGBA8888;
}

} // namespace domi
