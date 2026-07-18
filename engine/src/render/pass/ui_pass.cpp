#include "domi/pass/ui_pass.h"
#include "domi/render_command_buffer.h"
#include "domi/canvas2d.h"
#include "domi/ui/ui.h"
#include "domi/ui/font.h"
#include "domi/ui/clay_ui.h"
#include "domi/scene.h"
#include "domi/app.h"
#include "domi/input.h"
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

    // Small dark background pill behind the text.
    canvas->setFillColor(Color(0.0f, 0.0f, 0.0f, 0.6f));
    canvas->fillRect(8.0f, 8.0f, tw + 16.0f, th + 10.0f);

    font->drawText(canvas, 16.0f, 13.0f, text, Color(1.0f, 1.0f, 1.0f, 1.0f));
}

} // anonymous namespace

UIPass::~UIPass() {
    delete clayUI_;
}

void UIPass::record(CommandBuffer& cmd, RenderContext& ctx) {
    Canvas2D* canvas = cmd.getCanvas();
    if (!canvas) return;

    // Render directly to the screen after CompositePass.
    cmd.setTarget(NULL);

    // Render the active scene's declarative UI overlay.
    if (ctx.uiRoot && ctx.uiContext) {
        ctx.uiContext->render(canvas, *ctx.uiRoot);
    }

    // Clay-based UI declared by the active scene.
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
            clayUI_->endFrame(canvas);
        }
    }

    // Draw frame rate counter.
    if (ctx.fps > 0.0f) {
        drawFPS(canvas, ctx.fps);
    }
}

} // namespace domi
