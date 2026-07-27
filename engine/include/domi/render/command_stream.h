#ifndef DOMI_COMMAND_STREAM_H
#define DOMI_COMMAND_STREAM_H

#include <algorithm>
#include <functional>
#include <vector>

namespace domi {

// Shared record → replay buffer.
// RenderQueue / DrawBatch store plain ops; RenderList uses SortedCommandStream
// for layer/z ordering. Target is the type passed to each callback at flush.
template<typename Target>
class CommandStream {
public:
    typedef std::function<void(Target*)> Op;

    void clear() { ops_.clear(); }
    bool empty() const { return ops_.empty(); }
    size_t size() const { return ops_.size(); }

    void push(Op op) { ops_.push_back(std::move(op)); }

    // Replay in recording order without clearing (DrawBatch-style).
    void run(Target* target) const {
        if (!target) return;
        for (size_t i = 0; i < ops_.size(); ++i) {
            if (ops_[i]) ops_[i](target);
        }
    }

    // Replay then clear (RenderQueue-style).
    void flush(Target* target) {
        run(target);
        clear();
    }

protected:
    std::vector<Op> ops_;
};

// Recorded ops tagged with a sort key (e.g. layer + z).
// stable_sort keeps equal-key order so state ops stay sequenced.
template<typename Target, typename Key>
class SortedCommandStream {
public:
    typedef std::function<void(Target*)> Op;

    struct Item {
        Key key;
        Op fn;
    };

    void clear() { items_.clear(); }
    bool empty() const { return items_.empty(); }
    size_t size() const { return items_.size(); }

    void add(const Key& key, Op fn) {
        Item item;
        item.key = key;
        item.fn = std::move(fn);
        items_.push_back(std::move(item));
    }

    void sort() {
        std::stable_sort(items_.begin(), items_.end(),
                         [](const Item& a, const Item& b) {
                             return a.key < b.key;
                         });
    }

    void run(Target* target) const {
        if (!target) return;
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].fn) items_[i].fn(target);
        }
    }

    void flush(Target* target) {
        sort();
        run(target);
        clear();
    }

    std::vector<Item>& items() { return items_; }
    const std::vector<Item>& items() const { return items_; }

private:
    std::vector<Item> items_;
};

} // namespace domi

#endif // DOMI_COMMAND_STREAM_H
