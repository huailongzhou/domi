#include "domi/render_texture.h"

namespace domi {

RenderTexture::RenderTexture()
    : texture_(NULL), width_(0), height_(0), format_(SDL_PIXELFORMAT_UNKNOWN) {}

RenderTexture::~RenderTexture() {
    destroy();
}

bool RenderTexture::create(SDL_Renderer* renderer, int width, int height, SDL_PixelFormat format) {
    if (!renderer || width <= 0 || height <= 0) return false;

    destroy();

    texture_ = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!texture_) return false;

    width_ = width;
    height_ = height;
    format_ = format;

    // Default to blend so alpha channels work as expected.
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    return true;
}

void RenderTexture::destroy() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = NULL;
    }
    width_ = 0;
    height_ = 0;
    format_ = SDL_PIXELFORMAT_UNKNOWN;
}

} // namespace domi
