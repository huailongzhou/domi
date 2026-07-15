#include "domi/ui/font.h"
#include "domi/canvas2d.h"
#include "domi/material.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstring>
#include <vector>

namespace domi {

Font::Font()
    : library_(nullptr), face_(nullptr), pixelSize_(16) {
}

Font::~Font() {
    if (face_) {
        FT_Done_Face(face_);
        face_ = nullptr;
    }
    if (library_) {
        FT_Done_FreeType(library_);
        library_ = nullptr;
    }
}

bool Font::load(const char* path, int pixelSize) {
    if (library_ == nullptr) {
        if (FT_Init_FreeType(&library_) != 0) {
            library_ = nullptr;
            return false;
        }
    }

    if (face_) {
        FT_Done_Face(face_);
        face_ = nullptr;
    }

    FT_Face face = nullptr;
    if (FT_New_Face(library_, path, 0, &face) != 0) {
        return false;
    }

    if (FT_Set_Pixel_Sizes(face, 0, pixelSize) != 0) {
        FT_Done_Face(face);
        return false;
    }

    face_ = face;
    pixelSize_ = pixelSize;
    return true;
}

int Font::ascender() const {
    if (!face_) return pixelSize_;
    return face_->size->metrics.ascender >> 6;
}

int Font::descender() const {
    if (!face_) return 0;
    return face_->size->metrics.descender >> 6;
}

int Font::lineHeight() const {
    if (!face_) return pixelSize_;
    return face_->size->metrics.height >> 6;
}

void Font::measure(const char* text, float* outWidth, float* outHeight) const {
    if (!face_ || !text) {
        if (outWidth) *outWidth = 0.0f;
        if (outHeight) *outHeight = 0.0f;
        return;
    }

    int penX = 0;

    for (const char* p = text; *p; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (FT_Load_Char(face_, c, FT_LOAD_RENDER) != 0) {
            continue;
        }
        penX += static_cast<int>(face_->glyph->advance.x >> 6);
    }

    if (outWidth) *outWidth = static_cast<float>(penX);
    if (outHeight) *outHeight = static_cast<float>(lineHeight());
}

void Font::drawText(Canvas2D* canvas, float x, float y,
                    const char* text, const Color& color) const {
    if (!canvas || !face_ || !text || !*text) return;

    if (!text || !*text) return;

    float measureW, measureH;
    measure(text, &measureW, &measureH);
    if (measureW <= 0.0f || measureH <= 0.0f) return;

    const int width = static_cast<int>(measureW);
    const int height = lineHeight();
    const int base = ascender();

    std::vector<uint8_t> rgba(width * height * 4, 0);

    int penX = 0;
    for (const char* p = text; *p; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (FT_Load_Char(face_, c, FT_LOAD_RENDER) != 0) {
            continue;
        }
        FT_GlyphSlot g = face_->glyph;
        const int dstX = penX + g->bitmap_left;
        const int dstY = base - g->bitmap_top;
        const uint8_t* src = g->bitmap.buffer;
        const int srcPitch = g->bitmap.pitch;
        const int gw = static_cast<int>(g->bitmap.width);
        const int gh = static_cast<int>(g->bitmap.rows);

        for (int row = 0; row < gh; ++row) {
            int sy = dstY + row;
            if (sy < 0 || sy >= height) continue;
            for (int col = 0; col < gw; ++col) {
                int sx = dstX + col;
                if (sx < 0 || sx >= width) continue;
                uint8_t a = src[row * srcPitch + col];
                if (a == 0) continue;
                size_t idx = (sy * width + sx) * 4;
                rgba[idx + 0] = static_cast<uint8_t>(color.a * 255.0f * a / 255.0f);
                rgba[idx + 1] = static_cast<uint8_t>(color.r * 255.0f);
                rgba[idx + 2] = static_cast<uint8_t>(color.g * 255.0f);
                rgba[idx + 3] = static_cast<uint8_t>(color.b * 255.0f);
            }
        }

        penX += static_cast<int>(g->advance.x >> 6);
    }

    Material material(width, height, PixelFormat::ARGB8888);
    material.pixels = std::move(rgba);
    canvas->drawMaterial(x, y, material);
}

} // namespace domi
