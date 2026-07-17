#ifndef DOMI_RENDER_NODE_H
#define DOMI_RENDER_NODE_H

#include "domi/render_layer.h"
#include "domi/render_list.h"
#include "domi/draw_batch.h"
#include "domi/material.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <utility>

namespace domi {

class Canvas2D;
class Font;
class LayerView;

// How a node's sort key is derived when it is recorded into a RenderList.
enum class SortMode {
    Z,       // use the node's z value as-is (default)
    TopY,    // use the top edge (y1) of the node's y range
    CenterY, // use the vertical center (y1+y2)/2 of the y range
    BottomY  // use the bottom edge (y2) of the y range (classic ground y-sort)
};

// A node in the 2D scene render tree.
// Scenes are composed of RenderNodes. Containers (GroupNode, LayerView)
// own children via std::unique_ptr and pass the active RenderLayer down
// during build(). Leaf nodes record draw calls into the RenderList.
//
// Sorting: by default a node sorts by its public z value. Setting a SortMode
// makes the node derive its sort key from its own y range instead, so most
// scene code never has to specify z explicitly.
class RenderNode {
public:
    float z = 0.0f;
    SortMode sortMode = SortMode::Z;

    virtual ~RenderNode() {}
    virtual void build(RenderList& list, RenderLayer layer) const = 0;

    // The node's vertical extent in screen space. Default: a point at y=0.
    virtual void getYRange(float& y1, float& y2) const { y1 = 0.0f; y2 = 0.0f; }

    // The effective sort key used when this node is recorded.
    float sortKey() const {
        if (sortMode == SortMode::Z) return z;
        float y1, y2;
        getYRange(y1, y2);
        switch (sortMode) {
            case SortMode::TopY:    return y1;
            case SortMode::CenterY: return (y1 + y2) * 0.5f;
            case SortMode::BottomY: return y2;
            default:                return z;
        }
    }

    RenderNode& sortByY(SortMode mode) { sortMode = mode; return *this; }
    RenderNode& sortByTop()    { return sortByY(SortMode::TopY); }
    RenderNode& sortByCenterY(){ return sortByY(SortMode::CenterY); }
    RenderNode& sortByBottom() { return sortByY(SortMode::BottomY); }
};

// Generic container that passes the current layer through to children.
// Children are created in place and derive their sort key from their own
// y range (or keep z = 0):
//   GroupNode root;
//   root.addChild<RectNode>(...).sortByTop();
//   root.addChild<MaterialNode>(...).sortByBottom();
class GroupNode : public RenderNode {
public:
    GroupNode() {}

    // Base case for the variadic add.
    GroupNode& add() { return *this; }

    // Add child node values; each one is moved into a unique_ptr.
    GroupNode& add(std::unique_ptr<RenderNode> child) {
        children_.push_back(std::move(child));
        return *this;
    }

    template<typename T, typename... Rest>
    GroupNode& add(T&& node, Rest&&... rest) {
        addOne(std::forward<T>(node));
        return add(std::forward<Rest>(rest)...);
    }

    // Create a child node of type T in place with the given constructor
    // args. Returns a reference so the caller can set a sort strategy or
    // keep animating it.
    template<typename T, typename... Args>
    T& addChild(Args&&... args) {
        std::unique_ptr<T> child(new T(std::forward<Args>(args)...));
        T& ref = *child;
        children_.push_back(std::move(child));
        return ref;
    }

    // Layer factories: create a LayerView for the given layer and fill it
    // with the given child nodes.
    template<typename... Nodes>
    LayerView& backgroundLayer(Nodes&&... nodes);
    template<typename... Nodes>
    LayerView& groundLayer(Nodes&&... nodes);
    template<typename... Nodes>
    LayerView& surfaceLayer(Nodes&&... nodes);
    template<typename... Nodes>
    LayerView& objectLayer(Nodes&&... nodes);
    template<typename... Nodes>
    LayerView& canopyLayer(Nodes&&... nodes);
    template<typename... Nodes>
    LayerView& effectLayer(Nodes&&... nodes);
    template<typename... Nodes>
    LayerView& overlayLayer(Nodes&&... nodes);
    template<typename... Nodes>
    LayerView& uiLayer(Nodes&&... nodes);

    void build(RenderList& list, RenderLayer layer) const override {
        for (size_t i = 0; i < children_.size(); ++i) {
            children_[i]->build(list, layer);
        }
    }

private:
    std::vector<std::unique_ptr<RenderNode>> children_;

    void addOne(std::unique_ptr<RenderNode> child) {
        children_.push_back(std::move(child));
    }

    template<typename T>
    void addOne(T&& node) {
        children_.push_back(std::unique_ptr<RenderNode>(
            new typename std::decay<T>::type(std::move(node))));
    }

    template<typename... Nodes>
    LayerView& makeLayer(RenderLayer layer, Nodes&&... nodes);
};

// A container that forces a specific RenderLayer for all descendants.
class LayerView : public GroupNode {
public:
    explicit LayerView(RenderLayer layer) : layer_(layer) {}

    void build(RenderList& list, RenderLayer) const override {
        GroupNode::build(list, layer_);
    }

private:
    RenderLayer layer_;
};

template<typename... Nodes>
LayerView& GroupNode::makeLayer(RenderLayer layer, Nodes&&... nodes) {
    std::unique_ptr<LayerView> view(new LayerView(layer));
    LayerView& ref = *view;
    ref.add(std::forward<Nodes>(nodes)...);
    children_.push_back(std::move(view));
    return ref;
}

template<typename... Nodes>
LayerView& GroupNode::backgroundLayer(Nodes&&... nodes) {
    return makeLayer(RenderLayer::Background, std::forward<Nodes>(nodes)...);
}
template<typename... Nodes>
LayerView& GroupNode::groundLayer(Nodes&&... nodes) {
    return makeLayer(RenderLayer::Ground, std::forward<Nodes>(nodes)...);
}
template<typename... Nodes>
LayerView& GroupNode::surfaceLayer(Nodes&&... nodes) {
    return makeLayer(RenderLayer::Surface, std::forward<Nodes>(nodes)...);
}
template<typename... Nodes>
LayerView& GroupNode::objectLayer(Nodes&&... nodes) {
    return makeLayer(RenderLayer::Object, std::forward<Nodes>(nodes)...);
}
template<typename... Nodes>
LayerView& GroupNode::canopyLayer(Nodes&&... nodes) {
    return makeLayer(RenderLayer::Canopy, std::forward<Nodes>(nodes)...);
}
template<typename... Nodes>
LayerView& GroupNode::effectLayer(Nodes&&... nodes) {
    return makeLayer(RenderLayer::Effect, std::forward<Nodes>(nodes)...);
}
template<typename... Nodes>
LayerView& GroupNode::overlayLayer(Nodes&&... nodes) {
    return makeLayer(RenderLayer::Overlay, std::forward<Nodes>(nodes)...);
}
template<typename... Nodes>
LayerView& GroupNode::uiLayer(Nodes&&... nodes) {
    return makeLayer(RenderLayer::UI, std::forward<Nodes>(nodes)...);
}

// Filled rectangle.
class RectNode : public RenderNode {
public:
    RectNode(float x, float y, float w, float h, const Color& color)
        : x_(x), y_(y), w_(w), h_(h), color_(color) {}

    void getYRange(float& y1, float& y2) const override {
        y1 = y_;
        y2 = y_ + h_;
    }

    void build(RenderList& list, RenderLayer layer) const override {
        const float key = sortKey();
        list.setFillColor(layer, key, color_);
        list.fillRect(layer, key, x_, y_, w_, h_);
    }

    void setColor(const Color& c) { color_ = c; }

private:
    float x_, y_, w_, h_;
    Color color_;
};

// A material (sprite) drawn at a position.
class MaterialNode : public RenderNode {
public:
    MaterialNode(const Material& material, float x, float y, bool centered = false)
        : material_(&material), x_(x), y_(y), centered_(centered) {}

    void getYRange(float& y1, float& y2) const override {
        const float h = scaledHeight();
        const float bottom = pivotY();
        y1 = bottom - h;
        y2 = bottom;
    }

    void build(RenderList& list, RenderLayer layer) const override {
        if (!material_ || material_->width == 0) return;
        float dx = x_;
        float dy = y_;
        if (centered_) {
            dx -= material_->width * 0.5f;
            dy -= material_->height * 0.5f;
        }
        const float key = sortKey();
        if (scale_ == 1.0f) {
            list.drawMaterial(layer, key, dx, dy, *material_);
            return;
        }
        // Scale about the sprite's bottom-center pivot so feet/roots stay
        // planted (used for perspective scaling of scene props).
        const float px = pivotX();
        const float py = pivotY();
        list.save(layer, key);
        list.translate(layer, key, px, py);
        list.scale(layer, key, scale_, scale_);
        list.translate(layer, key, -px, -py);
        list.drawMaterial(layer, key, dx, dy, *material_);
        list.restore(layer, key);
    }

    void setPosition(float x, float y) { x_ = x; y_ = y; }
    void setCentered(bool c) { centered_ = c; }
    void setScale(float s) { scale_ = s; }

    float getX() const { return x_; }
    float getY() const { return y_; }
    bool isCentered() const { return centered_; }
    float getScale() const { return scale_; }
    const Material* getMaterial() const { return material_; }

private:
    // Bottom-center pivot in world coordinates.
    float pivotX() const {
        if (!material_) return x_;
        return centered_ ? x_ : x_ + material_->width * 0.5f;
    }
    float pivotY() const {
        if (!material_) return y_;
        return centered_ ? y_ + material_->height * 0.5f
                         : y_ + material_->height;
    }
    float scaledHeight() const {
        return material_ ? material_->height * scale_ : 0.0f;
    }

    const Material* material_;
    float x_, y_;
    bool centered_;
    float scale_ = 1.0f;
};

// A recorded path (moveTo/lineTo/curves/close) that can be filled and/or stroked.
class PathNode : public RenderNode {
public:
    PathNode& setFillColor(const Color& c) { fillColor_ = c; hasFill_ = true; return *this; }
    PathNode& setStrokeColor(const Color& c) { strokeColor_ = c; hasStroke_ = true; return *this; }
    PathNode& setLineWidth(float w) { lineWidth_ = w; return *this; }

    // Short fluent aliases.
    PathNode& fillColor(const Color& c) { return setFillColor(c); }
    PathNode& strokeColor(const Color& c) { return setStrokeColor(c); }
    PathNode& lineWidth(float w) { return setLineWidth(w); }

    PathNode& moveTo(float x, float y);
    PathNode& lineTo(float x, float y);
    PathNode& quadraticCurveTo(float cpx, float cpy, float x, float y);
    PathNode& bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);
    PathNode& closePath();

    // The y range covers all recorded points, including curve control points.
    void getYRange(float& y1, float& y2) const override;

    void build(RenderList& list, RenderLayer layer) const override;

private:
    struct Segment {
        enum Type { MoveTo, LineTo, QuadTo, BezierTo, Close } type;
        float x1, y1, x2, y2, x3, y3;
    };
    std::vector<Segment> segments_;
    Color fillColor_;
    Color strokeColor_;
    float lineWidth_ = 1.0f;
    bool hasFill_ = false;
    bool hasStroke_ = false;
};

// A stroked straight line.
class LineNode : public RenderNode {
public:
    LineNode(float x1, float y1, float x2, float y2, const Color& color)
        : x1_(x1), y1_(y1), x2_(x2), y2_(y2), color_(color) {}

    LineNode& setLineWidth(float w) { lineWidth_ = w; return *this; }
    LineNode& lineWidth(float w) { return setLineWidth(w); }

    void getYRange(float& y1, float& y2) const override {
        y1 = y1_ < y2_ ? y1_ : y2_;
        y2 = y1_ > y2_ ? y1_ : y2_;
    }

    void build(RenderList& list, RenderLayer layer) const override {
        const float key = sortKey();
        list.setStrokeColor(layer, key, color_);
        list.setLineWidth(layer, key, lineWidth_);
        list.drawLine(layer, key, x1_, y1_, x2_, y2_);
    }

    void setColor(const Color& c) { color_ = c; }
    void setEndpoints(float x1, float y1, float x2, float y2) {
        x1_ = x1; y1_ = y1; x2_ = x2; y2_ = y2;
    }

private:
    float x1_, y1_, x2_, y2_;
    Color color_;
    float lineWidth_ = 1.0f;
};

// An ellipse (or circle) that can be filled and/or stroked.
// By default it draws the full ellipse; setArc() restricts the sweep.
class EllipseNode : public RenderNode {
public:
    EllipseNode(float x, float y, float rx, float ry)
        : x_(x), y_(y), rx_(rx), ry_(ry) {}

    EllipseNode& setFillColor(const Color& c) { fillColor_ = c; hasFill_ = true; return *this; }
    EllipseNode& setStrokeColor(const Color& c) { strokeColor_ = c; hasStroke_ = true; return *this; }
    EllipseNode& setLineWidth(float w) { lineWidth_ = w; return *this; }

    EllipseNode& fillColor(const Color& c) { return setFillColor(c); }
    EllipseNode& strokeColor(const Color& c) { return setStrokeColor(c); }
    EllipseNode& lineWidth(float w) { return setLineWidth(w); }

    // Limit the sweep angles (radians). Defaults to a full ellipse.
    EllipseNode& setArc(float rotation, float startAngle, float endAngle, bool ccw = false) {
        rotation_ = rotation;
        startAngle_ = startAngle;
        endAngle_ = endAngle;
        ccw_ = ccw;
        return *this;
    }

    void getYRange(float& y1, float& y2) const override {
        y1 = y_ - ry_;
        y2 = y_ + ry_;
    }

    void build(RenderList& list, RenderLayer layer) const override {
        const float key = sortKey();
        if (hasFill_) {
            list.setFillColor(layer, key, fillColor_);
        }
        if (hasStroke_) {
            list.setStrokeColor(layer, key, strokeColor_);
            list.setLineWidth(layer, key, lineWidth_);
        }
        list.beginPath(layer, key);
        list.ellipse(layer, key, x_, y_, rx_, ry_, rotation_, startAngle_, endAngle_, ccw_);
        if (hasFill_) {
            list.fill(layer, key);
        }
        if (hasStroke_) {
            list.stroke(layer, key);
        }
    }

    void setCenter(float x, float y) { x_ = x; y_ = y; }
    void setRadius(float rx, float ry) { rx_ = rx; ry_ = ry; }

private:
    float x_, y_, rx_, ry_;
    float rotation_ = 0.0f;
    float startAngle_ = 0.0f;
    float endAngle_ = 2.0f * 3.14159265f;
    bool ccw_ = false;
    Color fillColor_;
    Color strokeColor_;
    float lineWidth_ = 1.0f;
    bool hasFill_ = false;
    bool hasStroke_ = false;
};

// A text string drawn with a Font.
class TextNode : public RenderNode {
public:
    TextNode(const char* text, Font* font, float x, float y, const Color& color)
        : text_(text ? text : ""), font_(font), x_(x), y_(y), color_(color) {}

    void getYRange(float& y1, float& y2) const override {
        y1 = y_;
        y2 = y_;
    }

    void build(RenderList& list, RenderLayer layer) const override {
        if (font_) {
            list.drawText(layer, sortKey(), x_, y_, text_.c_str(), font_, color_);
        }
    }

    void setText(const char* text) { text_ = text ? text : ""; }
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    void setColor(const Color& c) { color_ = c; }

private:
    std::string text_;
    Font* font_;
    float x_, y_;
    Color color_;
};

// A custom procedural draw node. The callback records a group of canvas
// calls into a DrawBatch; the batch is committed as a single item that
// shares this node's sort key and replays atomically at flush time.
// CustomNode has no intrinsic y range, so it sorts by z unless you give
// it a y range by deriving from it.
class CustomNode : public RenderNode {
public:
    using DrawFn = std::function<void(DrawBatch&)>;
    explicit CustomNode(DrawFn fn) : fn_(std::move(fn)) {}

    void build(RenderList& list, RenderLayer layer) const override {
        DrawBatch batch;
        fn_(batch);
        list.submit(layer, sortKey(), batch);
    }
private:
    DrawFn fn_;
};

} // namespace domi

#endif // DOMI_RENDER_NODE_H
