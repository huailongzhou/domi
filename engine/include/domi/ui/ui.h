#ifndef DOMI_UI_UI_H
#define DOMI_UI_UI_H

#include "domi/math.h"
#include "domi/canvas2d.h"
#include "domi/input.h"
#include <functional>
#include <string>
#include <vector>

namespace domi {

enum class UIAlignment {
    Start,
    Center,
    End,
    Stretch
};

// Lightweight styling for declarative UI widgets.
struct UIStyle {
    Color background = Color(0.5f, 0.5f, 0.5f, 1.0f);
    Color borderColor = Color(0.3f, 0.3f, 0.3f, 1.0f);
    Color foreground = Color(1.0f, 1.0f, 1.0f, 1.0f);
    float borderWidth = 4.0f;
    float cornerRadius = 10.0f;
    float padding = 0.0f;
};

// A single SwiftUI-like view node.
// Build a tree with VStack/HStack and leaf views (Button, Text, Spacer),
// then apply modifiers (background, border, frame, onClick, etc.).
class UIView {
public:
    enum Type {
        ViewEmpty,
        ViewButton,
        ViewStack,
        ViewText,
        ViewSpacer
    };

    Type type = Type::ViewEmpty;
    std::string text;
    std::function<void()> clickHandler;
    UIStyle style;

    float width = 0.0f;   // 0 = use parent/available size
    float height = 0.0f;
    UIAlignment align = UIAlignment::Center;
    float stackSpacing = 0.0f; // for stacks
    bool vertical = true; // true = VStack, false = HStack
    std::vector<UIView> children;

    // Layout results, filled by UIContext::render().
    mutable float computedX = 0.0f;
    mutable float computedY = 0.0f;
    mutable float computedW = 0.0f;
    mutable float computedH = 0.0f;

    // Modifiers - return *this for fluent chaining.
    UIView& background(const Color& c) { style.background = c; return *this; }
    UIView& border(const Color& c, float width) {
        style.borderColor = c;
        style.borderWidth = width;
        return *this;
    }
    UIView& foreground(const Color& c) { style.foreground = c; return *this; }
    UIView& cornerRadius(float r) { style.cornerRadius = r; return *this; }
    UIView& padding(float p) { style.padding = p; return *this; }
    UIView& frame(float w, float h) { width = w; height = h; return *this; }
    UIView& alignment(UIAlignment a) { align = a; return *this; }
    UIView& spacing(float s) { stackSpacing = s; return *this; }
    UIView& onClick(std::function<void()> cb) { clickHandler = std::move(cb); return *this; }

    // Factory helpers.
    static UIView Button(const char* label);
    static UIView Text(const char* label);
    static UIView Spacer();
    static UIView VStack(float spacing, std::vector<UIView> children);
    static UIView HStack(float spacing, std::vector<UIView> children);

private:
    // Layout helpers used by UIContext.
    void layout(float x, float y, float availableW, float availableH) const;
    void render(Canvas2D* canvas) const;
    bool hitTest(float x, float y) const;

    friend class UIContext;
};

// Renders a declarative UIView tree and dispatches clicks.
class UIContext {
public:
    void render(Canvas2D* canvas, const UIView& root) const;
    bool handleClick(float x, float y, const UIView& root) const;
};

} // namespace domi

#endif // DOMI_UI_UI_H
