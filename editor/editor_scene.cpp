#include "editor_scene.h"

#include "domi/app.h"
#include "domi/render.h"
#include "domi/canvas2d.h"
#include "domi/render_node.h"
#include "tree_node.h"
#include "tree_generator.h"
#include "cloud_generator.h"
#include "rock_generator.h"
#include "house_generator.h"
#include "horizon_generator.h"

#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

using nlohmann::json;

namespace {

domi::Color specColor(const json& spec, const char* key, const domi::Color& def) {
    if (!spec.contains(key) || !spec[key].is_array() || spec[key].size() < 3) return def;
    const json& c = spec[key];
    return domi::Color(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(),
                       c.size() > 3 ? c[3].get<float>() : 1.0f);
}

// Editing helpers. Each returns true when the value changed.

bool dragFloat(const char* label, json& node, const char* key, float speed = 1.0f) {
    if (!node.contains(key) || !node[key].is_number()) return false;
    float v = node[key].get<float>();
    if (ImGui::DragFloat(label, &v, speed)) { node[key] = v; return true; }
    return false;
}

bool dragInt(const char* label, json& node, const char* key, float speed = 1.0f) {
    if (!node.contains(key) || !node[key].is_number()) return false;
    int v = node[key].get<int>();
    if (ImGui::DragInt(label, &v, (int)speed > 0 ? (int)speed : 1)) { node[key] = v; return true; }
    return false;
}

bool textField(const char* label, json& node, const char* key) {
    if (!node.contains(key) || !node[key].is_string()) return false;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", node[key].get<std::string>().c_str());
    ImGui::PushID(key);
    bool changed = ImGui::InputText(label, buf, sizeof(buf));
    ImGui::PopID();
    if (changed) node[key] = buf;
    return changed;
}

bool colorEdit(const char* label, json& node, const char* key) {
    if (!node.contains(key) || !node[key].is_array() || node[key].size() < 3) return false;
    json& c = node[key];
    while (c.size() < 4) c.push_back(1.0f);
    float col[4] = { c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>() };
    ImGui::PushID(key);
    bool changed = ImGui::ColorEdit4(label, col);
    ImGui::PopID();
    if (changed) { c[0] = col[0]; c[1] = col[1]; c[2] = col[2]; c[3] = col[3]; }
    return changed;
}

void previewMaterial(domi::Canvas2D* canvas, const std::string& name, const domi::Material& mat) {
    if (!canvas || mat.width <= 0 || mat.height <= 0) return;
    std::string key = "editor:preview:" + name;
    void* handle = canvas->uploadMaterial(key.c_str(), mat);
    if (!handle) return;
    float scale = 96.0f / (float)mat.height;
    float w = mat.width * scale, h = mat.height * scale;
    if (w > 200.0f) { scale = 200.0f / (float)mat.width; w = 200.0f; h = mat.height * scale; }
    ImGui::Image((ImTextureID)handle, ImVec2(w, h));
}

} // namespace

EditorScene::EditorScene()
    : selNode_(nullptr), selParent_(nullptr), selIndex_(-1) {
    snprintf(pathBuf_, sizeof(pathBuf_), "assets/scenes/game2d.json");
}

void EditorScene::load(domi::World* world, domi::ScriptSystem* script) {
    (void)world; (void)script;
    loadFile(pathBuf_);
}

void EditorScene::unload(domi::World* world, domi::ScriptSystem* script) {
    (void)world; (void)script;
    setRootNode(nullptr);
    clearSelection();
}

void EditorScene::update(double dt) {
    (void)dt;
}

void EditorScene::loadFile(const std::string& path) {
    // Try the path as typed, then relative to a couple of parent dirs so the
    // editor works from the project root or a build directory.
    const char* prefixes[] = { "", "../", "../../" };
    std::ifstream in;
    std::string opened;
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        in.open(prefixes[i] + path);
        if (in) { opened = prefixes[i] + path; break; }
    }
    if (!in) {
        fprintf(stderr, "[EDITOR] Cannot open '%s'\n", path.c_str());
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    json doc = json::parse(ss.str(), nullptr, false);
    if (doc.is_discarded()) {
        fprintf(stderr, "[EDITOR] '%s' is not valid JSON\n", path.c_str());
        return;
    }
    doc_ = std::move(doc);
    filePath_ = opened;
    ensureMaterialsSection();
    generateMaterials();
    clearSelection();
    rebuildTree();
    fprintf(stderr, "[EDITOR] Loaded '%s'\n", opened.c_str());
}

void EditorScene::saveFile(const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        fprintf(stderr, "[EDITOR] Cannot write '%s'\n", path.c_str());
        return;
    }
    out << doc_.dump(2) << "\n";
    fprintf(stderr, "[EDITOR] Saved '%s'\n", path.c_str());
}

void EditorScene::ensureMaterialsSection() {
    if (doc_.contains("materials") && doc_["materials"].is_object()) return;

    // Default specs matching the demo scene's procedural materials.
    json mats = json::object();
    mats["horizon"] = {{"generator", "horizon"}, {"seed", 5000}, {"w", 2560}, {"h", 120},
        {"base", {0.28, 0.72, 0.28, 1.0}}, {"sky", {0.12, 0.12, 0.16, 1.0}},
        {"hill", {0.15, 0.40, 0.15, 1.0}}, {"hillCount", 5}, {"hillMin", 25}, {"hillMax", 70}};
    mats["house"] = {{"generator", "house"}, {"seed", 3000}, {"w", 90}, {"h", 90},
        {"base", {0.9, 0.85, 0.6, 1.0}}, {"detail", {0.65, 0.25, 0.2, 1.0}},
        {"wallW", 55}, {"wallH", 45}, {"roofH", 28}};
    for (int i = 0; i < 6; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "cloud%d", i);
        mats[name] = {{"generator", "cloud"}, {"seed", 4000 + i * 211}, {"w", 120}, {"h", 80},
            {"base", {0.95, 0.95, 0.98, 1.0}}, {"puffs", 5}, {"puffRadius", 34}};
    }
    for (int i = 0; i < 4; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "rock%d", i);
        mats[name] = {{"generator", "rock"}, {"seed", 2000 + i * 93}, {"w", 50}, {"h", 40},
            {"base", {0.55, 0.55, 0.55, 1.0}}, {"rocks", 4}, {"rockRadius", 16}};
    }
    for (int i = 0; i < 10; ++i) {
        char pair[32], trunk[32], foliage[32];
        snprintf(pair, sizeof(pair), "tree%d", i);
        snprintf(trunk, sizeof(trunk), "trunk%d", i);
        snprintf(foliage, sizeof(foliage), "foliage%d", i);
        mats[pair] = {{"generator", "tree"}, {"seed", 1000 + i * 137}, {"w", 80}, {"h", 80},
            {"base", {0.15, 0.55, 0.15, 1.0}}, {"highlight", {0.22, 0.68, 0.22, 1.0}},
            {"detail", {0.45, 0.28, 0.16, 1.0}}, {"trunkW", 14}, {"trunkH", 32},
            {"foliageRadius", 30}, {"trunk", trunk}, {"foliage", foliage}};
    }
    doc_["materials"] = mats;
}

void EditorScene::generateMaterials() {
    materials_.clear();
    if (!doc_.contains("materials") || !doc_["materials"].is_object()) return;
    for (auto it = doc_["materials"].begin(); it != doc_["materials"].end(); ++it) {
        generateMaterial(it.key(), it.value());
    }
}

void EditorScene::generateMaterial(const std::string& name, const json& spec) {
    using namespace domi;
    const std::string gen = spec.value("generator", "");
    const int w = spec.value("w", 80);
    const int h = spec.value("h", 80);
    const unsigned seed = spec.value("seed", 0u);
    const Color base = specColor(spec, "base", Color(1, 1, 1, 1));

    if (gen == "cloud") {
        materials_[name] = CloudGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setPuffCount(spec.value("puffs", 5))
            .setPuffRadius(spec.value("puffRadius", 34))
            .build();
    } else if (gen == "rock") {
        materials_[name] = RockGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setRockCount(spec.value("rocks", 4))
            .setRockRadius(spec.value("rockRadius", 16))
            .build();
    } else if (gen == "house") {
        materials_[name] = HouseGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setDetailColor(specColor(spec, "detail", Color(0.65f, 0.25f, 0.2f, 1.0f)))
            .setWallSize(spec.value("wallW", 55), spec.value("wallH", 45))
            .setRoofHeight(spec.value("roofH", 28))
            .build();
    } else if (gen == "horizon") {
        materials_[name] = HorizonGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setSkyColor(specColor(spec, "sky", Color(0.12f, 0.12f, 0.16f, 1.0f)))
            .setHillColor(specColor(spec, "hill", Color(0.15f, 0.4f, 0.15f, 1.0f)))
            .setHillCount(spec.value("hillCount", 5))
            .setHillHeight(spec.value("hillMin", 25), spec.value("hillMax", 70))
            .build();
    } else if (gen == "tree") {
        domi::Material trunk, foliage;
        TreeGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setHighlightColor(specColor(spec, "highlight", Color(0.22f, 0.68f, 0.22f, 1.0f)))
            .setDetailColor(specColor(spec, "detail", Color(0.45f, 0.28f, 0.16f, 1.0f)))
            .setTrunkSize(spec.value("trunkW", 14), spec.value("trunkH", 32))
            .setFoliageRadius(spec.value("foliageRadius", 30))
            .buildPair(trunk, foliage);
        materials_[spec.value("trunk", name + ".trunk")] = trunk;
        materials_[spec.value("foliage", name + ".foliage")] = foliage;
    }
}

void EditorScene::rebuildTree() {
    using namespace domi;
    SceneLoader loader;
    loader.setMaterialResolver([this](const std::string& name) -> const Material* {
        std::map<std::string, Material>::iterator it = materials_.find(name);
        return it != materials_.end() ? &it->second : nullptr;
    });
    loader.registerType("cloud", [this](const json& j, const SceneLoader& l)
            -> std::unique_ptr<RenderNode> {
        const Material* material = l.resolveMaterial(j.value("material", ""));
        if (!material) return nullptr;
        std::unique_ptr<MaterialNode> node(new MaterialNode(
            *material, j.value("x", 0.0f), j.value("y", 0.0f), true));
        node->sortByCenterY();
        return node;
    });
    loader.registerType("tree", [this](const json& j, const SceneLoader& l)
            -> std::unique_ptr<RenderNode> {
        std::unique_ptr<TreeNode> node(new TreeNode());
        node->trunk = l.resolveMaterial(j.value("trunk", ""));
        node->foliage = l.resolveMaterial(j.value("foliage", ""));
        node->x = j.value("x", 0.0f);
        node->y = j.value("y", 0.0f);
        float t = (node->y - 240.0f) / 480.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        node->scale = 0.35f + t * 0.65f;
        return node;
    });
    setRootNode(loader.loadFromJson(doc_));
}

void EditorScene::selectNode(json& node, json& parent, int index) {
    selNode_ = &node;
    selParent_ = &parent;
    selIndex_ = index;
}

void EditorScene::clearSelection() {
    selNode_ = nullptr;
    selParent_ = nullptr;
    selIndex_ = -1;
}

std::string EditorScene::nodeLabel(const json& node, int index) const {
    const std::string type = node.value("type", "?");
    if (node.contains("id")) return type + "  " + node["id"].get<std::string>();
    if (node.contains("material")) return type + "  " + node["material"].get<std::string>();
    if (type == "layer") return "layer  " + node.value("layer", "");
    char buf[48];
    snprintf(buf, sizeof(buf), "%s  #%d", type.c_str(), index);
    return buf;
}

void EditorScene::hierarchyChildren(json& node) {
    if (!node.contains("children") || !node["children"].is_array()) return;
    json& children = node["children"];
    for (int i = 0; i < (int)children.size(); ++i) {
        json& child = children[i];
        ImGui::PushID(&child);
        const std::string label = nodeLabel(child, i);
        if (child.contains("children")) {
            bool open = ImGui::TreeNodeEx(label.c_str(),
                (child.value("type", "") == "layer") ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                selectNode(child, children, i);
            }
            if (open) {
                if (child.value("type", "") == "layer") {
                    if (ImGui::SmallButton("+ material")) {
                        child["children"].push_back(
                            {{"type", "material"}, {"material", "house"},
                             {"x", 640.0f}, {"y", 360.0f},
                             {"centered", true}, {"sort", "bottom"}});
                        rebuildTree();
                    }
                }
                hierarchyChildren(child);
                ImGui::TreePop();
            }
        } else {
            if (ImGui::Selectable(label.c_str(), selNode_ == &child)) {
                selectNode(child, children, i);
            }
        }
        ImGui::PopID();
    }
}

void EditorScene::panelFiles() {
    ImGui::Text("Scene file:");
    ImGui::PushItemWidth(-1.0f);
    ImGui::InputText("##scenepath", pathBuf_, sizeof(pathBuf_));
    ImGui::PopItemWidth();
    if (ImGui::Button("Load")) loadFile(pathBuf_);
    ImGui::SameLine();
    if (ImGui::Button("Save")) saveFile(filePath_.empty() ? pathBuf_ : filePath_);
    ImGui::SameLine();
    if (ImGui::Button("Save As")) saveFile(pathBuf_);
}

void EditorScene::panelHierarchy() {
    if (doc_.contains("root")) {
        hierarchyChildren(doc_["root"]);
    }
}

void EditorScene::panelProperties() {
    if (!selNode_) {
        ImGui::TextDisabled("Select a node in the tree above");
        return;
    }
    json& n = *selNode_;
    const std::string type = n.value("type", "");
    ImGui::Text("type: %s", type.c_str());

    bool changed = false;
    if (n.contains("id")) changed |= textField("id", n, "id");
    if (n.contains("z")) changed |= dragFloat("z", n, "z");
    if (n.contains("sort")) {
        const char* modes[] = { "z", "top", "center", "bottom" };
        const std::string cur = n["sort"].get<std::string>();
        int idx = 0;
        for (int i = 0; i < 4; ++i) if (cur == modes[i]) idx = i;
        if (ImGui::Combo("sort", &idx, modes, 4)) { n["sort"] = modes[idx]; changed = true; }
    }

    if (type == "material") {
        changed |= dragFloat("x", n, "x");
        changed |= dragFloat("y", n, "y");
        changed |= textField("material", n, "material");
        if (n.contains("centered")) {
            bool c = n["centered"].get<bool>();
            if (ImGui::Checkbox("centered", &c)) { n["centered"] = c; changed = true; }
        }
    } else if (type == "rect") {
        changed |= dragFloat("x", n, "x");
        changed |= dragFloat("y", n, "y");
        changed |= dragFloat("w", n, "w");
        changed |= dragFloat("h", n, "h");
        changed |= colorEdit("color", n, "color");
    } else if (type == "line") {
        changed |= dragFloat("x1", n, "x1");
        changed |= dragFloat("y1", n, "y1");
        changed |= dragFloat("x2", n, "x2");
        changed |= dragFloat("y2", n, "y2");
        changed |= colorEdit("color", n, "color");
        changed |= dragFloat("lineWidth", n, "lineWidth", 0.1f);
    } else if (type == "ellipse") {
        changed |= dragFloat("x", n, "x");
        changed |= dragFloat("y", n, "y");
        changed |= dragFloat("rx", n, "rx");
        changed |= dragFloat("ry", n, "ry");
        changed |= colorEdit("fill", n, "fill");
        changed |= colorEdit("stroke", n, "stroke");
        changed |= dragFloat("lineWidth", n, "lineWidth", 0.1f);
    } else if (type == "cloud") {
        changed |= dragFloat("x", n, "x");
        changed |= dragFloat("y", n, "y");
        changed |= dragFloat("speed", n, "speed", 0.01f);
        changed |= textField("material", n, "material");
    } else if (type == "tree") {
        changed |= dragFloat("x", n, "x");
        changed |= dragFloat("y", n, "y");
        changed |= textField("trunk", n, "trunk");
        changed |= textField("foliage", n, "foliage");
    } else if (type == "path") {
        int segs = n.contains("segments") ? (int)n["segments"].size() : 0;
        ImGui::Text("path: %d segments (read-only in v1)", segs);
        changed |= colorEdit("fill", n, "fill");
        changed |= colorEdit("stroke", n, "stroke");
    } else if (type == "layer") {
        ImGui::Text("layer: %s", n.value("layer", "").c_str());
    }

    if (changed) rebuildTree();

    ImGui::Separator();
    if (ImGui::Button("Delete node")) {
        if (selParent_ && selIndex_ >= 0) {
            selParent_->erase(selParent_->begin() + selIndex_);
            clearSelection();
            rebuildTree();
        }
    }
}

void EditorScene::panelMaterials() {
    if (!doc_.contains("materials")) return;
    json& mats = doc_["materials"];

    ImGui::Text("Material specs (click to edit):");
    ImGui::BeginChild("matlist", ImVec2(0, 140), true);
    for (auto it = mats.begin(); it != mats.end(); ++it) {
        const std::string& name = it.key();
        if (ImGui::Selectable(name.c_str(), selectedMaterial_ == name)) {
            selectedMaterial_ = name;
        }
    }
    ImGui::EndChild();

    if (selectedMaterial_.empty() || !mats.contains(selectedMaterial_)) return;
    json& spec = mats[selectedMaterial_];
    const std::string gen = spec.value("generator", "");
    ImGui::Text("generator: %s", gen.c_str());

    bool changed = false;
    changed |= dragInt("seed", spec, "seed");
    changed |= dragInt("w", spec, "w");
    changed |= dragInt("h", spec, "h");
    changed |= colorEdit("base", spec, "base");

    if (gen == "cloud") {
        changed |= dragInt("puffs", spec, "puffs");
        changed |= dragInt("puffRadius", spec, "puffRadius");
    } else if (gen == "rock") {
        changed |= dragInt("rocks", spec, "rocks");
        changed |= dragInt("rockRadius", spec, "rockRadius");
    } else if (gen == "house") {
        changed |= colorEdit("detail", spec, "detail");
        changed |= dragInt("wallW", spec, "wallW");
        changed |= dragInt("wallH", spec, "wallH");
        changed |= dragInt("roofH", spec, "roofH");
    } else if (gen == "horizon") {
        changed |= colorEdit("sky", spec, "sky");
        changed |= colorEdit("hill", spec, "hill");
        changed |= dragInt("hillCount", spec, "hillCount");
        changed |= dragInt("hillMin", spec, "hillMin");
        changed |= dragInt("hillMax", spec, "hillMax");
    } else if (gen == "tree") {
        changed |= colorEdit("highlight", spec, "highlight");
        changed |= colorEdit("detail", spec, "detail");
        changed |= dragInt("trunkW", spec, "trunkW");
        changed |= dragInt("trunkH", spec, "trunkH");
        changed |= dragInt("foliageRadius", spec, "foliageRadius");
    }

    domi::Canvas2D* canvas = domi::App::instance().getRender()->getCanvas2D();
    if (gen == "tree") {
        const std::string trunkName = spec.value("trunk", "");
        const std::string foliageName = spec.value("foliage", "");
        if (materials_.count(trunkName)) {
            ImGui::Text("trunk: %s", trunkName.c_str());
            previewMaterial(canvas, trunkName, materials_[trunkName]);
        }
        if (materials_.count(foliageName)) {
            ImGui::Text("foliage: %s", foliageName.c_str());
            previewMaterial(canvas, foliageName, materials_[foliageName]);
        }
    } else if (materials_.count(selectedMaterial_)) {
        previewMaterial(canvas, selectedMaterial_, materials_[selectedMaterial_]);
    }

    if (changed || ImGui::Button("Regenerate")) {
        generateMaterial(selectedMaterial_, spec);
        rebuildTree();
    }
}

void EditorScene::drawEditor() {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(380, 900), ImGuiCond_Always);
    ImGui::Begin("Domi Editor", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    if (ImGui::CollapsingHeader("Files", ImGuiTreeNodeFlags_DefaultOpen)) panelFiles();
    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) panelHierarchy();
    if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen)) panelProperties();
    if (ImGui::CollapsingHeader("Materials")) panelMaterials();
    ImGui::End();
}
