#include "domi/ui/font.h"
#include "domi/render/canvas2d.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>
#include <utility>
#include <vector>

namespace domi {

Font::Font()
    : library_(nullptr), face_(nullptr), pixelSize_(16),
      atlas_(nullptr), atlasW_(0), atlasH_(0),
      shelfX_(0), shelfY_(0), shelfH_(0), atlasFull_(false) {
}

Font::~Font() {
    // The atlas texture belongs to the backend and is released with it.
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

bool Font::ensureAtlas(Canvas2D* canvas) {
    if (atlas_ || atlasFull_) return atlas_ != nullptr;
    if (!canvas) return false;

    // Smallest power-of-two atlas whose per-glyph cells fit the printable
    // ASCII range with room to spare.
    const int cellW = pixelSize_ + 2;
    const int cellH = pixelSize_ + 6;
    int size = 128;
    while (size < 4096 && (size / cellW) * (size / cellH) < 96) {
        size *= 2;
    }

    atlas_ = canvas->createMutableTexture(size, size);
    if (!atlas_) {
        atlasFull_ = true;  // do not retry every frame
        fprintf(stderr, "[Font] failed to create glyph atlas\n");
        return false;
    }
    atlasW_ = atlasH_ = size;
    return true;
}

const Font::Glyph* Font::glyphFor(Canvas2D* canvas, unsigned char c) {
    std::unordered_map<unsigned char, Glyph>::iterator it = glyphs_.find(c);
    if (it != glyphs_.end()) return &it->second;
    if (!face_ || atlasFull_) return nullptr;
    if (!ensureAtlas(canvas)) return nullptr;

    if (FT_Load_Char(face_, c, FT_LOAD_RENDER) != 0) {
        return nullptr;
    }
    FT_GlyphSlot g = face_->glyph;

    Glyph gl;
    gl.w = static_cast<int>(g->bitmap.width);
    gl.h = static_cast<int>(g->bitmap.rows);
    gl.bearingX = g->bitmap_left;
    gl.bearingY = g->bitmap_top;
    gl.advance = static_cast<int>(g->advance.x >> 6);
    gl.atlasX = 0;
    gl.atlasY = 0;

    if (gl.w > 0 && gl.h > 0) {
        // Shelf packing with a 1px gutter between cells.
        if (shelfX_ + gl.w + 1 > atlasW_) {
            shelfX_ = 0;
            shelfY_ += shelfH_ + 1;
            shelfH_ = 0;
        }
        if (shelfY_ + gl.h + 1 > atlasH_) {
            atlasFull_ = true;
            fprintf(stderr, "[Font] glyph atlas full (%dpx font)\n", pixelSize_);
            return nullptr;
        }
        gl.atlasX = shelfX_;
        gl.atlasY = shelfY_;
        if (gl.h > shelfH_) shelfH_ = gl.h;
        shelfX_ += gl.w + 1;

        // White RGB + coverage alpha; the draw-time tint supplies the color.
        std::vector<uint8_t> rgba((size_t)gl.w * gl.h * 4);
        const uint8_t* src = g->bitmap.buffer;
        const int srcPitch = g->bitmap.pitch;
        for (int row = 0; row < gl.h; ++row) {
            uint8_t* dst = rgba.data() + (size_t)row * gl.w * 4;
            const uint8_t* s = src + (size_t)row * srcPitch;
            for (int col = 0; col < gl.w; ++col) {
                dst[col * 4 + 0] = 255;
                dst[col * 4 + 1] = 255;
                dst[col * 4 + 2] = 255;
                dst[col * 4 + 3] = s[col];
            }
        }
        canvas->updateTextureRegion(atlas_, gl.atlasX, gl.atlasY,
                                    gl.w, gl.h, rgba.data());
    }

    return &glyphs_.insert(std::make_pair(c, gl)).first->second;
}

void Font::drawText(Canvas2D* canvas, float x, float y,
                    const char* text, const Color& color) {
    if (!canvas || !face_ || !text || !*text) return;

    const int base = ascender();
    float penX = x;
    for (const char* p = text; *p; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        const Glyph* gl = glyphFor(canvas, c);
        if (!gl) continue;
        if (gl->w > 0 && gl->h > 0) {
            float dx = penX + static_cast<float>(gl->bearingX);
            float dy = y + static_cast<float>(base - gl->bearingY);
            canvas->drawMaterialRegion(atlas_, gl->atlasX, gl->atlasY,
                                       gl->w, gl->h, dx, dy, color);
        }
        penX += static_cast<float>(gl->advance);
    }
}

} // namespace domi
