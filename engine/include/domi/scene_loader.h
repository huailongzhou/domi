#ifndef DOMI_SCENE_LOADER_H
#define DOMI_SCENE_LOADER_H

#include "domi/third_party/json.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace domi {

class GroupNode;
class Material;
class RenderNode;

// Loads a RenderNode tree from a JSON scene file.
//
// File format:
//   {
//     "root": {
//       "type": "group",
//       "children": [
//         { "type": "layer", "layer": "background", "children": [ ... ] }
//       ]
//     }
//   }
//
// Built-in node types: "group", "layer", "rect", "material", "path",
// "line", "ellipse". Common properties on every node:
//   "id"   - optional name; the node can be looked up later via find()
//   "z"    - explicit sort key (default 0)
//   "sort" - "z" (default) | "top" | "center" | "bottom"
//
// Scene-specific node types (e.g. a demo's "tree" node) are added with
// registerType(); materials are resolved by name through the resolver.
class SceneLoader {
public:
    // Resolves a material name to the Material it refers to (not owned).
    using MaterialResolver = std::function<const Material*(const std::string& name)>;
    // Builds a node of a custom type from its JSON object. Receives the
    // loader so custom factories can resolve materials too.
    using NodeFactory = std::function<std::unique_ptr<RenderNode>(
        const nlohmann::json& node, const SceneLoader& loader)>;

    void setMaterialResolver(MaterialResolver resolver) { resolver_ = std::move(resolver); }
    void registerType(const std::string& type, NodeFactory factory) {
        factories_[type] = std::move(factory);
    }

    const Material* resolveMaterial(const std::string& name) const {
        return resolver_ ? resolver_(name) : nullptr;
    }

    // Loads the file and returns the root group. Returns nullptr on error
    // (the reason is printed to stderr).
    std::unique_ptr<GroupNode> load(const char* path);

    // Builds the tree from an already-parsed document (must contain "root").
    std::unique_ptr<GroupNode> loadFromJson(const nlohmann::json& doc);

    // Returns the node that was loaded with the given "id", or nullptr.
    RenderNode* find(const std::string& id) const {
        std::unordered_map<std::string, RenderNode*>::const_iterator it = ids_.find(id);
        return it != ids_.end() ? it->second : nullptr;
    }

private:
    std::unique_ptr<RenderNode> buildNode(const nlohmann::json& j);
    void buildChildren(const nlohmann::json& j, GroupNode& parent);
    void applyCommon(const nlohmann::json& j, RenderNode& node);

    MaterialResolver resolver_;
    std::unordered_map<std::string, NodeFactory> factories_;
    std::unordered_map<std::string, RenderNode*> ids_;
};

} // namespace domi

#endif // DOMI_SCENE_LOADER_H
