#ifndef DOMI_UI_FONT_H
#define DOMI_UI_FONT_H

#include "domi/math.h"
#include <string>

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace domi {

class Canvas2D;

// A lightweight FreeType-based font.
// Loads a TTF file at a fixed pixel size and renders strings as materials
// so text goes through the same backend/queue path as other 2D drawing.
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
                  const char* text, const Color& color) const;

private:
    FT_LibraryRec_* library_;
    FT_FaceRec_* face_;
    int pixelSize_;

    int ascender() const;
    int descender() const;
    int lineHeight() const;
};

} // namespace domi

#endif // DOMI_UI_FONT_H
