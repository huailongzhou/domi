#include "domi/render/pass/ui_pass.h"
#include "domi/render/canvas2d.h"
#include "domi/ui/ui.h"
#include "domi/ui/font.h"
#include "domi/ui/clay_ui.h"
#include "domi/scene/scene.h"
#include "domi/core/app.h"
#include "domi/input/input.h"
#include <cstdio>
#include <cstring>

namespace domi {

namespace {

Font* getUIFont() {
    static Font font;
    if (!font.loaded()) {
        const char* paths[] = {
            "assets/fonts/default.ttf",
            "../assets/fonts/default.ttf",
            "default.ttf",
            nullptr
        };
        for (int i = 0; paths[i]; ++i) {
            if (font.load(paths[i], 20)) {
                break;
            }
        }
        if (!font.loaded()) {
            fprintf(stderr, "[UIPass] Failed to load default font\n");
        }
    }
    return &font;
}

void drawFPS(Canvas2D* canvas, float fps) {
    Font* font = getUIFont();
    if (!font || !font->loaded()) return;

    char text[32];
    std::snprintf(text, sizeof(text), "FPS: %.0f", fps);

    float tw = 0.0f, th = 0.0f;
    font->measure(text, &tw, &th);

    canvas->setFillColor(Color(0.0f, 0.0f, 0.0f, 0.6f));
    canvas->fillRect(8.0f, 8.0f, tw + 16.0f, th + 10.0f);

    font->drawText(canvas, 16.0f, 13.0f, text, Color(1.0f, 1.0f, 1.0f, 1.0f));
}

} // anonymous namespace

UIPass::~UIPass() {
    delete clayUI_;
}

void UIPass::record(Canvas2D& canvas, RenderContext& ctx) {
    canvas.setRenderTarget(NULL);

    if (ctx.uiRoot && ctx.uiContext) {
        ctx.uiContext->render(&canvas, *ctx.uiRoot);
    }

    if (ctx.scene) {
        if (!clayUI_) {
            clayUI_ = new ClayUI();
            if (!clayUI_->init()) {
                delete clayUI_;
                clayUI_ = NULL;
            }
        }
        if (clayUI_) {
            InputSystem* input = App::instance().getInput();
            const float mx = input ? input->getMouseX() : -1.0f;
            const float my = input ? input->getMouseY() : -1.0f;
            const bool down = input && input->isMouseButtonDown(1);
            clayUI_->beginFrame(static_cast<float>(ctx.width),
                                static_cast<float>(ctx.height), mx, my, down);
            ctx.scene->buildClayUI(*clayUI_);
            clayUI_->endFrame(&canvas);
        }
    }

    if (ctx.fps > 0.0f) {
        drawFPS(&canvas, ctx.fps);
    }
}

} // namespace domi
