#ifndef DOMI_UI_FONT_H
#define DOMI_UI_FONT_H

#include "domi/core/math.h"
#include <string>
#include <unordered_map>

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace domi {

class Canvas2D;

// A lightweight FreeType-based font.
// Loads a TTF file at a fixed pixel size and draws text through a per-font
// glyph atlas: each unique glyph is rasterized and uploaded into one shared
// backend texture exactly once (as white RGB + coverage alpha), afterwards
// drawing text only queues sub-rect draws of the atlas. This means dynamic
// text (FPS counters, HUD numbers) never creates new GPU textures at
// runtime, and one atlas serves all colors because tinting happens at draw
// time via texture modulation.
//
// The atlas texture is owned by the backend and is freed when the backend
// is destroyed.
class Font {
public:
    Font();
    ~Font();

    // Load a font file. Returns false on error.
    bool load(const char* path, int pixelSize);
    bool loaded() const { return face_ != nullptr; }

    // Pixel metrics for a UTF-8 string (ASCII-only for now).
    void measure(const char* text, float* outWidth, float* outHeight) const;

    // Draw text with its top-left corner at (x, y).
    void drawText(Canvas2D* canvas, float x, float y,
                  const char* text, const Color& color);

private:
    struct Glyph {
        int atlasX, atlasY;  // top-left inside the atlas texture
        int w, h;            // bitmap size in pixels (0 for whitespace)
        int bearingX, bearingY;
        int advance;
    };

    // Rasterize + upload the glyph on first use, then return the cached entry.
    const Glyph* glyphFor(Canvas2D* canvas, unsigned char c);
    bool ensureAtlas(Canvas2D* canvas);

    FT_LibraryRec_* library_;
    FT_FaceRec_* face_;
    int pixelSize_;

    void* atlas_;  // backend mutable-texture handle (owned by the backend)
    int atlasW_, atlasH_;
    int shelfX_, shelfY_, shelfH_;  // simple shelf packer state
    bool atlasFull_;
    std::unordered_map<unsigned char, Glyph> glyphs_;

    int ascender() const;
    int descender() const;
    int lineHeight() const;
};

} // namespace domi

#endif // DOMI_UI_FONT_H
