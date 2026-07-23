#include "domi/scene_loader.h"
#include "domi/render_node.h"
#include "domi/material.h"
#include <cstdio>
#include <fstream>
#include <sstream>

using nlohmann::json;

namespace domi {

namespace {

float getFloat(const json& j, const char* key, float def = 0.0f) {
    return j.value(key, def);
}

Color getColor(const json& j, const char* key, const Color& def = Color(1, 1, 1, 1)) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3) return def;
    const json& c = j[key];
    return Color(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(),
                 c.size() > 3 ? c[3].get<float>() : 1.0f);
}

bool layerFromName(const std::string& name, RenderLayer& out) {
    if (name == "background") { out = RenderLayer::Background; return true; }
    if (name == "ground")     { out = RenderLayer::Ground;     return true; }
    if (name == "surface")    { out = RenderLayer::Surface;    return true; }
    if (name == "shadow")     { out = RenderLayer::Shadow;     return true; }
    if (name == "object")     { out = RenderLayer::Object;     return true; }
    if (name == "canopy")     { out = RenderLayer::Canopy;     return true; }
    if (name == "effect")     { out = RenderLayer::Effect;     return true; }
    if (name == "overlay")    { out = RenderLayer::Overlay;    return true; }
    if (name == "ui")         { out = RenderLayer::UI;         return true; }
    return false;
}

} // namespace

std::unique_ptr<GroupNode> SceneLoader::load(const char* path) {
    std::ifstream in(path);
    if (!in) {
        fprintf(stderr, "[SCENE_LOADER] Cannot open '%s'\n", path);
        return nullptr;
    }
    std::stringstream ss;
    ss << in.rdbuf();

    json doc = json::parse(ss.str(), nullptr, false);
    if (doc.is_discarded()) {
        fprintf(stderr, "[SCENE_LOADER] '%s' is not valid JSON\n", path);
        return nullptr;
    }
    return loadFromJson(doc);
}

std::unique_ptr<GroupNode> SceneLoader::loadFromJson(const json& doc) {
    ids_.clear();
    materialNodes_.clear();

    if (!doc.contains("root")) {
        fprintf(stderr, "[SCENE_LOADER] document has no \"root\" node\n");
        return nullptr;
    }

    std::unique_ptr<RenderNode> root = buildNode(doc["root"]);
    if (!root) return nullptr;

    GroupNode* group = dynamic_cast<GroupNode*>(root.get());
    if (!group) {
        fprintf(stderr, "[SCENE_LOADER] root must be a group or layer\n");
        return nullptr;
    }
    root.release();
    return std::unique_ptr<GroupNode>(group);
}

void SceneLoader::applyCommon(const json& j, RenderNode& node) {
    if (j.contains("z")) node.z = j["z"].get<float>();
    if (j.contains("sort")) {
        const std::string mode = j["sort"].get<std::string>();
        if (mode == "top")         node.sortByTop();
        else if (mode == "center") node.sortByCenterY();
        else if (mode == "bottom") node.sortByBottom();
        // "z" (or anything else) keeps the default z sort.
    }
    if (j.contains("id")) ids_[j["id"].get<std::string>()] = &node;
}

void SceneLoader::buildChildren(const json& j, GroupNode& parent) {
    if (!j.contains("children") || !j["children"].is_array()) return;
    for (size_t i = 0; i < j["children"].size(); ++i) {
        std::unique_ptr<RenderNode> child = buildNode(j["children"][i]);
        if (child) parent.add(std::move(child));
    }
}

std::unique_ptr<RenderNode> SceneLoader::buildNode(const json& j) {
    const std::string type = j.value("type", "");
    std::unique_ptr<RenderNode> node;

    if (type == "group") {
        std::unique_ptr<GroupNode> group(new GroupNode());
        buildChildren(j, *group);
        node = std::move(group);
    } else if (type == "layer") {
        RenderLayer layer;
        if (!layerFromName(j.value("layer", ""), layer)) {
            fprintf(stderr, "[SCENE_LOADER] Unknown layer '%s'\n",
                    j.value("layer", "").c_str());
            return nullptr;
        }
        std::unique_ptr<LayerView> view(new LayerView(layer));
        buildChildren(j, *view);
        node = std::move(view);
    } else if (type == "rect") {
        node.reset(new RectNode(getFloat(j, "x"), getFloat(j, "y"),
                                getFloat(j, "w"), getFloat(j, "h"),
                                getColor(j, "color")));
    } else if (type == "material") {
        const std::string name = j.value("material", "");
        const Material* material = resolveMaterial(name);
        if (!material) {
            fprintf(stderr, "[SCENE_LOADER] Unknown material '%s', node skipped\n",
                    name.c_str());
            return nullptr;
        }
        MaterialNode* matNode = new MaterialNode(*material, getFloat(j, "x"), getFloat(j, "y"),
                                                 j.value("centered", false));
        matNode->setCastShadow(j.value("castShadow", true));
        materialNodes_.push_back(std::make_pair(name, matNode));
        node.reset(matNode);
    } else if (type == "path") {
        std::unique_ptr<PathNode> path(new PathNode());
        if (j.contains("fill")) path->setFillColor(getColor(j, "fill"));
        if (j.contains("stroke")) path->setStrokeColor(getColor(j, "stroke"));
        if (j.contains("lineWidth")) path->setLineWidth(j["lineWidth"].get<float>());
        if (j.contains("segments") && j["segments"].is_array()) {
            for (size_t i = 0; i < j["segments"].size(); ++i) {
                const json& seg = j["segments"][i];
                if (!seg.is_array() || seg.empty()) continue;
                const std::string op = seg[0].get<std::string>();
                if (op == "m" && seg.size() >= 3) {
                    path->moveTo(seg[1].get<float>(), seg[2].get<float>());
                } else if (op == "l" && seg.size() >= 3) {
                    path->lineTo(seg[1].get<float>(), seg[2].get<float>());
                } else if (op == "q" && seg.size() >= 5) {
                    path->quadraticCurveTo(seg[1].get<float>(), seg[2].get<float>(),
                                           seg[3].get<float>(), seg[4].get<float>());
                } else if (op == "b" && seg.size() >= 7) {
                    path->bezierCurveTo(seg[1].get<float>(), seg[2].get<float>(),
                                        seg[3].get<float>(), seg[4].get<float>(),
                                        seg[5].get<float>(), seg[6].get<float>());
                } else if (op == "z") {
                    path->closePath();
                }
            }
        }
        node = std::move(path);
    } else if (type == "line") {
        std::unique_ptr<LineNode> line(new LineNode(
            getFloat(j, "x1"), getFloat(j, "y1"),
            getFloat(j, "x2"), getFloat(j, "y2"), getColor(j, "color")));
        if (j.contains("lineWidth")) line->setLineWidth(j["lineWidth"].get<float>());
        node = std::move(line);
    } else if (type == "ellipse") {
        std::unique_ptr<EllipseNode> ellipse(new EllipseNode(
            getFloat(j, "x"), getFloat(j, "y"),
            getFloat(j, "rx"), getFloat(j, "ry")));
        if (j.contains("fill")) ellipse->setFillColor(getColor(j, "fill"));
        if (j.contains("stroke")) ellipse->setStrokeColor(getColor(j, "stroke"));
        if (j.contains("lineWidth")) ellipse->setLineWidth(j["lineWidth"].get<float>());
        if (j.contains("arc") && j["arc"].is_array() && j["arc"].size() >= 3) {
            const json& arc = j["arc"];
            ellipse->setArc(arc[0].get<float>(), arc[1].get<float>(),
                            arc[2].get<float>(),
                            arc.size() > 3 ? arc[3].get<bool>() : false);
        }
        node = std::move(ellipse);
    } else {
        std::unordered_map<std::string, NodeFactory>::const_iterator it =
            factories_.find(type);
        if (it == factories_.end()) {
            fprintf(stderr, "[SCENE_LOADER] Unknown node type '%s', skipped\n",
                    type.c_str());
            return nullptr;
        }
        node = it->second(j, *this);
        if (!node) return nullptr;
    }

    applyCommon(j, *node);
    return node;
}

} // namespace domi
