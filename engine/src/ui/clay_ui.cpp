#include "domi/ui/clay_ui.h"

#include "clay_layout.h"
#include "domi/canvas2d.h"
#include "domi/ui/font.h"

#include <cstdio>

namespace domi {

ClayUI::ClayUI()
    : prevMouseDown_(false), releasedThisFrame_(false) {}

ClayUI::~ClayUI() {
    for (std::map<int, Font*>::iterator it = fonts_.begin(); it != fonts_.end(); ++it) {
        delete it->second;
    }
    claylay_shutdown();
}

bool ClayUI::init() {
    const char* paths[] = {
        "assets/fonts/default.ttf",
        "../assets/fonts/default.ttf",
        "../../assets/fonts/default.ttf",
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        FILE* f = fopen(paths[i], "rb");
        if (!f) continue;
        fclose(f);
        fontPath_ = paths[i];
        break;
    }
    if (fontPath_.empty()) {
        fprintf(stderr, "[CLAY_UI] default.ttf not found\n");
        return false;
    }
    if (!claylay_init()) return false;
    claylay_set_measure(measureCallback, this);
    return true;
}

Font* ClayUI::fontForSize(int size) {
    std::map<int, Font*>::iterator it = fonts_.find(size);
    if (it != fonts_.end()) return it->second;
    Font* font = new Font();
    if (!font->load(fontPath_.c_str(), size)) {
        delete font;
        return NULL;
    }
    fonts_[size] = font;
    return font;
}

void ClayUI::measureCallback(const char* text, int fontSize,
                             float* outW, float* outH, void* userData) {
    ClayUI* self = static_cast<ClayUI*>(userData);
    if (!self || !outW || !outH) return;
    Font* font = self->fontForSize(fontSize > 0 ? fontSize : 20);
    if (!font) {
        *outW = 0.0f;
        *outH = 0.0f;
        return;
    }
    font->measure(text, outW, outH);
}

void ClayUI::beginFrame(float width, float height, float mouseX, float mouseY, bool mouseDown) {
    releasedThisFrame_ = prevMouseDown_ && !mouseDown;
    prevMouseDown_ = mouseDown;
    claylay_begin(width, height, mouseX, mouseY, mouseDown ? 1 : 0);
}

bool ClayUI::hovered(const char* id) const {
    return claylay_hovered(id) != 0;
}

bool ClayUI::clicked(const char* id) const {
    return releasedThisFrame_ && claylay_hovered(id) != 0;
}

void ClayUI::beginBox(const Box& box) {
    ClayLayBox b;
    b.id = box.id;
    b.width = box.width;
    b.height = box.height;
    b.horizontal = box.horizontal ? 1 : 0;
    b.childGap = box.childGap;
    b.alignX = box.alignX;
    b.alignY = box.alignY;
    b.paddingL = box.paddingL;
    b.paddingR = box.paddingR;
    b.paddingT = box.paddingT;
    b.paddingB = box.paddingB;
    b.bgR = box.background.r * 255.0f;
    b.bgG = box.background.g * 255.0f;
    b.bgB = box.background.b * 255.0f;
    b.bgA = box.background.a * 255.0f;
    b.borderR = box.borderColor.r * 255.0f;
    b.borderG = box.borderColor.g * 255.0f;
    b.borderB = box.borderColor.b * 255.0f;
    b.borderA = box.borderColor.a * 255.0f;
    b.borderWidth = box.borderWidth;
    b.cornerRadius = box.cornerRadius;
    claylay_box_open(&b);
}

void ClayUI::endBox() {
    claylay_box_close();
}

void ClayUI::text(const char* text, int fontSize, const Color& color) {
    claylay_text(text, fontSize,
                 color.r * 255.0f, color.g * 255.0f,
                 color.b * 255.0f, color.a * 255.0f);
}

void ClayUI::endFrame(Canvas2D* canvas) {
    claylay_end();
    if (!canvas) return;

    int count = 0;
    const ClayLayCmd* cmds = claylay_commands(&count);
    for (int i = 0; i < count; ++i) {
        const ClayLayCmd& c = cmds[i];
        const Color color(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
        switch (c.type) {
            case CLAYLAY_CMD_RECT:
                canvas->setFillColor(color);
                canvas->fillRect(c.x, c.y, c.w, c.h);
                break;
            case CLAYLAY_CMD_BORDER:
                if (c.lineWidth <= 0.0f) break;
                canvas->setStrokeColor(color);
                canvas->setLineWidth(c.lineWidth);
                canvas->strokeRect(c.x, c.y, c.w, c.h);
                break;
            case CLAYLAY_CMD_TEXT: {
                Font* font = fontForSize(c.fontSize);
                if (!font) break;
                font->drawText(canvas, c.x, c.y, c.text, color);
                break;
            }
            case CLAYLAY_CMD_CLIP_START:
                canvas->setClipRect(c.x, c.y, c.w, c.h);
                break;
            case CLAYLAY_CMD_CLIP_END:
                canvas->resetClipRect();
                break;
            default:
                break;
        }
    }
}

} // namespace domi
