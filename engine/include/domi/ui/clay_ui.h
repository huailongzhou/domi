#ifndef DOMI_UI_CLAY_UI_H
#define DOMI_UI_CLAY_UI_H

#include "domi/math.h"
#include <map>
#include <string>

namespace domi {

class Canvas2D;
class Font;

// C++11 API over the Clay layout engine. clay.h itself is compiled inside a
// C99 translation unit (engine/src/ui/clay_layout.c) and never leaks into
// C++ headers, so all C++ files stay C++11.
//
// Per frame (driven by UIPass):
//   ui.beginFrame(w, h, mouseX, mouseY, mouseDown);
//   ui.beginBox(root); ... ui.text(...); ui.endBox();
//   ui.endFrame(canvas);
//
// Interaction during declaration: hovered(id) is live; clicked(id) is true
// on the frame the mouse button was released over the element.
class ClayUI {
public:
    // Box (container) declaration. Colors are engine 0-1 floats.
    // width/height <= 0 means "fit content".
    struct Box {
        const char* id = nullptr;
        float width = 0.0f;
        float height = 0.0f;
        bool horizontal = false; // false = vertical stack
        int childGap = 0;
        int alignX = 1;          // 0 left, 1 center, 2 right
        int alignY = 1;          // 0 top, 1 center, 2 bottom
        int paddingL = 0, paddingR = 0, paddingT = 0, paddingB = 0;
        Color background = Color(0.0f, 0.0f, 0.0f, 0.0f);
        Color borderColor = Color(0.0f, 0.0f, 0.0f, 0.0f);
        int borderWidth = 0;
        float cornerRadius = 0.0f; // layout-only for now (no rounded raster)
    };

    ClayUI();
    ~ClayUI();

    // Initializes the Clay context and locates the default font (same prefix
    // search as the scene files).
    bool init();

    void beginFrame(float width, float height, float mouseX, float mouseY, bool mouseDown);
    // Runs the layout and draws the resulting commands.
    void endFrame(Canvas2D* canvas);

    void beginBox(const Box& box);
    void endBox();
    void text(const char* text, int fontSize, const Color& color);

    bool hovered(const char* id) const;
    bool clicked(const char* id) const;

    // Loads (and caches) the default font at the given pixel size.
    Font* fontForSize(int size);

private:
    static void measureCallback(const char* text, int fontSize,
                                float* outW, float* outH, void* userData);

    std::map<int, Font*> fonts_; // per pixel size, owned
    std::string fontPath_;
    bool prevMouseDown_;
    bool releasedThisFrame_;
};

} // namespace domi

#endif // DOMI_UI_CLAY_UI_H
