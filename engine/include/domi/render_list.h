#ifndef DOMI_RENDER_LIST_H
#define DOMI_RENDER_LIST_H

#include "domi/render_layer.h"
#include <vector>
#include <functional>
#include <algorithm>

namespace domi {

// A lightweight deferred 2D draw list.
// Items are recorded with a layer and a y-coordinate, then sorted and executed
// on flush. This makes 2D scene ordering independent of call order.
class RenderList {
public:
    using DrawFn = std::function<void()>;

    struct Item {
        RenderLayer layer;
        float z;
        DrawFn draw;
    };

    void clear() { items_.clear(); }

    void add(RenderLayer layer, float z, const DrawFn& draw) {
        Item item;
        item.layer = layer;
        item.z = z;
        item.draw = draw;
        items_.push_back(item);
    }

    void flush() {
        std::sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) {
            if (a.layer != b.layer) {
                return static_cast<int>(a.layer) < static_cast<int>(b.layer);
            }
            return a.z < b.z;
        });
        for (size_t i = 0; i < items_.size(); ++i) {
            items_[i].draw();
        }
    }

    size_t size() const { return items_.size(); }

private:
    std::vector<Item> items_;
};

} // namespace domi

#endif // DOMI_RENDER_LIST_H
