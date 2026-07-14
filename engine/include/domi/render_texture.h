#ifndef DOMI_RENDER_TEXTURE_H
#define DOMI_RENDER_TEXTURE_H

#include "domi/backend/render_backend.h"

namespace domi {

// A render-target texture used by the RenderPass pipeline.
// The native handle is opaque to the engine and is managed by the active
// IRenderBackend implementation.
class RenderTexture {
public:
    RenderTexture();
    ~RenderTexture();

    // Create a texture of the given size and format. Returns true on success.
    bool create(IRenderBackend* backend, int width, int height,
                RenderTextureFormat format = RenderTextureFormat::RGBA8888);

    // Destroy the underlying native texture.
    void destroy();

    bool valid() const { return handle_ != NULL; }

    void* getNative() const { return handle_; }

    int width() const { return width_; }
    int height() const { return height_; }
    RenderTextureFormat format() const { return format_; }

private:
    IRenderBackend* backend_;
    void* handle_;
    int width_;
    int height_;
    RenderTextureFormat format_;
};

} // namespace domi

#endif // DOMI_RENDER_TEXTURE_H
