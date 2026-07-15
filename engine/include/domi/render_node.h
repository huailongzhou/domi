#ifndef DOMI_RENDER_NODE_H
#define DOMI_RENDER_NODE_H

#include "domi/render_layer.h"
#include "domi/render_list.h"
#include "domi/draw_batch.h"
#include "domi/material.h"
#include <vector>
#include <memory>
#include <functional>
#include <utility>

namespace domi {

class Canvas2D;
class LayerView;

// A node in the 2D scene render tree.
// Scenes are composed of RenderNodes. Containers (GroupNode, LayerView)
// own children via std::unique_ptr and pass the active RenderLayer down
// during build(). Leaf nodes record draw calls into the RenderList.
class RenderNode {
public:
    float z = 0.0f;
    virtual ~RenderNode() {}
    virtual void build(RenderList& list, RenderLayer layer) const = 0;
};

// Generic container that passes the current layer through to children.
// Children can be added one by one or all at once:
//   GroupNode root;
//   root.add(RectNode(...).setZ(1.0f), PathNode().setZ(2.0f)...);
class GroupNode : public RenderNode {
public:
    GroupNode() {}

    GroupNode& setZ(float z_) { z = z_; return *this; }

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

    // Create a child node of type T in place with the given z and
    // constructor args. Returns a reference so the caller can keep
    // animating it.
    template<typename T, typename... Args>
    T& addChild(float z, Args&&... args) {
        std::unique_ptr<T> child(new T(std::forward<Args>(args)...));
        child->z = z;
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

    LayerView& setZ(float z_) { z = z_; return *this; }

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

    RectNode& setZ(float z_) { z = z_; return *this; }

    void build(RenderList& list, RenderLayer layer) const override {
        list.setFillColor(layer, z, color_);
        list.fillRect(layer, z, x_, y_, w_, h_);
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

    MaterialNode& setZ(float z_) { z = z_; return *this; }

    void build(RenderList& list, RenderLayer layer) const override {
        if (!material_ || material_->width == 0) return;
        float dx = x_;
        float dy = y_;
        if (centered_) {
            dx -= material_->width * 0.5f;
            dy -= material_->height * 0.5f;
        }
        list.drawMaterial(layer, z, dx, dy, *material_);
    }

    void setPosition(float x, float y) { x_ = x; y_ = y; }
    void setCentered(bool c) { centered_ = c; }

private:
    const Material* material_;
    float x_, y_;
    bool centered_;
};

// A recorded path (moveTo/lineTo/curves/close) that can be filled and/or stroked.
class PathNode : public RenderNode {
public:
    PathNode& setZ(float z_) { z = z_; return *this; }

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

// A custom procedural draw node. The callback records a group of canvas
// calls into a DrawBatch; the batch is committed as a single item that
// shares this node's z and replays atomically at flush time.
class CustomNode : public RenderNode {
public:
    using DrawFn = std::function<void(DrawBatch&)>;
    explicit CustomNode(DrawFn fn) : fn_(std::move(fn)) {}

    CustomNode& setZ(float z_) { z = z_; return *this; }

    void build(RenderList& list, RenderLayer layer) const override {
        DrawBatch batch;
        fn_(batch);
        list.submit(layer, z, batch);
    }
private:
    DrawFn fn_;
};

} // namespace domi

#endif // DOMI_RENDER_NODE_H
