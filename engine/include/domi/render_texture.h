#ifndef DOMI_RENDER_TEXTURE_H
#define DOMI_RENDER_TEXTURE_H

#include <SDL3/SDL.h>

namespace domi {

// A render-target texture used by the RenderPass pipeline.
// Wraps an SDL_Texture created with SDL_TEXTUREACCESS_TARGET.
class RenderTexture {
public:
    RenderTexture();
    ~RenderTexture();

    // Create a texture of the given size and SDL pixel format.
    // Returns true on success.
    bool create(SDL_Renderer* renderer, int width, int height, SDL_PixelFormat format);

    // Destroy the underlying SDL texture.
    void destroy();

    bool valid() const { return texture_ != NULL; }

    SDL_Texture* getNative() const { return texture_; }

    int width() const { return width_; }
    int height() const { return height_; }
    SDL_PixelFormat format() const { return format_; }

private:
    SDL_Texture* texture_;
    int width_;
    int height_;
    SDL_PixelFormat format_;
};

} // namespace domi

#endif // DOMI_RENDER_TEXTURE_H
