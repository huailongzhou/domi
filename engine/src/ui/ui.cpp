#include "domi/ui/ui.h"
#include "domi/backend/render_backend.h"
#include "domi/ui/font.h"
#include <cmath>
#include <cstdio>

namespace domi {

UIView UIView::Button(const char* label) {
    UIView v;
    v.type = Type::ViewButton;
    v.text = label ? label : "";
    return v;
}

UIView UIView::Text(const char* label) {
    UIView v;
    v.type = Type::ViewText;
    v.text = label ? label : "";
    return v;
}

UIView UIView::Spacer() {
    UIView v;
    v.type = Type::ViewSpacer;
    return v;
}

UIView UIView::VStack(float spacing, std::vector<UIView> children) {
    UIView v;
    v.type = Type::ViewStack;
    v.vertical = true;
    v.stackSpacing = spacing;
    v.children = std::move(children);
    return v;
}

UIView UIView::HStack(float spacing, std::vector<UIView> children) {
    UIView v;
    v.type = Type::ViewStack;
    v.vertical = false;
    v.stackSpacing = spacing;
    v.children = std::move(children);
    return v;
}

namespace {

const float PI = 3.14159265f;

Font* getDefaultFont() {
    static Font font;
    if (!font.loaded()) {
        const char* paths[] = {
            "assets/fonts/default.ttf",
            "../assets/fonts/default.ttf",
            "default.ttf",
            nullptr
        };
        for (int i = 0; paths[i]; ++i) {
            if (font.load(paths[i], 24)) {
                break;
            }
        }
        if (!font.loaded()) {
            fprintf(stderr, "[UI] Failed to load default font\n");
        }
    }
    return &font;
}

void drawRoundedRect(Canvas2D* canvas, float x, float y,
                     float w, float h, float radius) {
    float r = radius;
    canvas->beginPath();
    canvas->moveTo(x + r, y);
    canvas->arcTo(x + w, y,     x + w, y + h, r);
    canvas->arcTo(x + w, y + h, x,     y + h, r);
    canvas->arcTo(x,     y + h, x,     y,     r);
    canvas->arcTo(x,     y,     x + w, y,     r);
    canvas->closePath();
    canvas->fill();
}

float alignOffset(float available, float size, UIAlignment a) {
    switch (a) {
        case UIAlignment::Start:    return 0.0f;
        case UIAlignment::End:      return available - size;
        case UIAlignment::Center:
        case UIAlignment::Stretch:
        default:                    return (available - size) * 0.5f;
    }
}

} // anonymous namespace

void UIView::layout(float x, float y, float availableW, float availableH) const {
    computedX = x;
    computedY = y;
    computedW = width > 0.0f ? width : availableW;
    computedH = height > 0.0f ? height : availableH;

    if (type != Type::ViewStack || children.empty()) return;

    // Determine how much space the fixed-size children need along the stack axis.
    float fixedMain = 0.0f;
    int flexCount = 0;
    float crossSize = vertical ? computedW : computedH;

    for (size_t i = 0; i < children.size(); ++i) {
        const UIView& c = children[i];
        float main = vertical ? c.height : c.width;
        if (main <= 0.0f || c.type == Type::ViewSpacer) {
            ++flexCount;
        } else {
            fixedMain += main;
        }
    }

    float totalSpacing = stackSpacing * std::max(0, (int)children.size() - 1);
    float remainingMain = (vertical ? computedH : computedW) - fixedMain - totalSpacing;
    if (remainingMain < 0.0f) remainingMain = 0.0f;
    float flexSize = flexCount > 0 ? remainingMain / flexCount : 0.0f;

    float mainOffset = 0.0f;
    switch (align) {
        case UIAlignment::End:      mainOffset = remainingMain; break;
        case UIAlignment::Center:   mainOffset = remainingMain * 0.5f; break;
        case UIAlignment::Start:
        case UIAlignment::Stretch:
        default:                    mainOffset = 0.0f; break;
    }

    float pos = (vertical ? y : x) + mainOffset;
    for (size_t i = 0; i < children.size(); ++i) {
        const UIView& c = children[i];
        float childMain = vertical ? c.height : c.width;
        if (childMain <= 0.0f || c.type == Type::ViewSpacer) {
            childMain = flexSize;
        }

        float childCross = vertical ? c.width : c.height;
        if (childCross <= 0.0f || c.align == UIAlignment::Stretch) {
            childCross = crossSize;
        }

        float offset = alignOffset(crossSize, childCross, c.align);

        if (vertical) {
            c.layout(x + offset, pos, childCross, childMain);
        } else {
            c.layout(pos, y + offset, childMain, childCross);
        }

        pos += childMain + stackSpacing;
    }
}

void UIView::render(Canvas2D* canvas) const {
    if (!canvas) return;

    switch (type) {
        case Type::ViewButton: {
            const UIStyle& s = style;
            float bw = s.borderWidth;
            float pad = s.padding;

            // Border.
            canvas->setFillColor(s.borderColor);
            drawRoundedRect(canvas,
                            computedX - bw, computedY - bw,
                            computedW + bw * 2.0f, computedH + bw * 2.0f,
                            s.cornerRadius + bw);

            // Background.
            canvas->setFillColor(s.background);
            drawRoundedRect(canvas,
                            computedX + pad, computedY + pad,
                            computedW - pad * 2.0f, computedH - pad * 2.0f,
                            s.cornerRadius);

            // Label.
            if (!text.empty()) {
                Font* font = getDefaultFont();
                if (font && font->loaded()) {
                    float tw = 0.0f, th = 0.0f;
                    font->measure(text.c_str(), &tw, &th);
                    float tx = computedX + (computedW - tw) * 0.5f;
                    float ty = computedY + (computedH - th) * 0.5f;
                    font->drawText(canvas, tx, ty, text.c_str(), s.foreground);
                }
            }
            break;
        }
        case Type::ViewText: {
            if (!text.empty()) {
                Font* font = getDefaultFont();
                if (font && font->loaded()) {
                    font->drawText(canvas, computedX, computedY, text.c_str(), style.foreground);
                }
            }
            break;
        }
        case Type::ViewStack: {
            for (size_t i = 0; i < children.size(); ++i) {
                children[i].render(canvas);
            }
            break;
        }
        case Type::ViewSpacer:
        case Type::ViewEmpty:
        default:
            break;
    }
}

bool UIView::hitTest(float x, float y) const {
    return x >= computedX && x < computedX + computedW &&
           y >= computedY && y < computedY + computedH;
}

void UIContext::render(Canvas2D* canvas, const UIView& root) const {
    if (!canvas) return;
    float w = static_cast<float>(canvas->getBackend() ? canvas->getBackend()->getWidth() : 1280);
    float h = static_cast<float>(canvas->getBackend() ? canvas->getBackend()->getHeight() : 720);
    root.layout(0.0f, 0.0f, w, h);
    root.render(canvas);
}

bool UIContext::handleClick(float x, float y, const UIView& root) const {
    // A simple depth-first search for the topmost button under (x, y).
    // We rely on the layout having already been computed by render().
    const UIView* hit = nullptr;

    std::function<void(const UIView&)> search = [&](const UIView& node) {
        if (!node.hitTest(x, y)) return;
        if (node.type == UIView::Type::ViewButton && node.clickHandler) {
            hit = &node;
        }
        for (size_t i = 0; i < node.children.size(); ++i) {
            search(node.children[i]);
        }
    };

    search(root);
    if (hit && hit->clickHandler) {
        hit->clickHandler();
        return true;
    }
    return false;
}

} // namespace domi
