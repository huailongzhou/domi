#ifndef DOMI_UI_FONT_H
#define DOMI_UI_FONT_H

#include "domi/math.h"
#include "domi/material.h"
#include <list>
#include <string>
#include <unordered_map>

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace domi {

class Canvas2D;

// A lightweight FreeType-based font.
// Loads a TTF file at a fixed pixel size and renders strings as materials
// so text goes through the same backend/queue path as other 2D drawing.
//
// Rasterized text materials are cached by (string, color) so repeated draws
// of the same text do not re-run FreeType glyph loading.
class Font {
public:
    Font();
    ~Font();

    // Load a font file. Returns false on error.
    bool load(const char* path, int pixelSize);
    bool loaded() const { return face_ != nullptr; }

    // Pixel metrics for a UTF-8 string (ASCII-only for now).
    void measure(const char* text, float* outWidth, float* outHeight) const;

    // Returns true if a material for this text+color is already cached.
    bool checkMaterial(const char* text, const Color& color) const;

    // Draw text with its top-left corner at (x, y).
    void drawText(Canvas2D* canvas, float x, float y,
                  const char* text, const Color& color);

    // Clear the rasterized material cache.
    void clearCache();

private:
    FT_LibraryRec_* library_;
    FT_FaceRec_* face_;
    int pixelSize_;

    struct CachedText {
        Material material;
        float width = 0.0f;
        float height = 0.0f;
    };

    mutable std::list<std::string> lru_;
    mutable std::unordered_map<std::string, std::pair<CachedText, std::list<std::string>::iterator>> cache_;

    static std::string makeKey(const char* text, const Color& color);
    const CachedText* findCache(const char* text, const Color& color) const;
    void insertCache(const char* text, const Color& color, const CachedText& entry) const;

    int ascender() const;
    int descender() const;
    int lineHeight() const;
};

} // namespace domi

#endif // DOMI_UI_FONT_H
