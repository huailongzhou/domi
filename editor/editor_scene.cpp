#include "editor_scene.h"

#include "domi/app.h"
#include "domi/render.h"
#include "domi/canvas2d.h"
#include "domi/render_node.h"
#include "domi/scene_function.h"
#include "domi/material_io.h"
#include "tree_generator.h"
#include "cloud_generator.h"
#include "rock_generator.h"
#include "house_generator.h"
#include "horizon_generator.h"

#include "imgui.h"

#include <SDL3/SDL_filesystem.h>

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

// Runs the generator described by a spec and returns the material
// (empty for an unknown generator).
domi::Material generateFromSpec(const json& spec) {
    using namespace domi;
    const std::string gen = spec.value("generator", "");
    const int w = spec.value("w", 80);
    const int h = spec.value("h", 80);
    const unsigned seed = spec.value("seed", 0u);
    const Color base = specColor(spec, "base", Color(1, 1, 1, 1));

    if (gen == "cloud") {
        return CloudGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setPuffCount(spec.value("puffs", 5))
            .setPuffRadius(spec.value("puffRadius", 34))
            .build();
    }
    if (gen == "rock") {
        return RockGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setRockCount(spec.value("rocks", 4))
            .setRockRadius(spec.value("rockRadius", 16))
            .build();
    }
    if (gen == "house") {
        return HouseGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setDetailColor(specColor(spec, "detail", Color(0.65f, 0.25f, 0.2f, 1.0f)))
            .setWallSize(spec.value("wallW", 55), spec.value("wallH", 45))
            .setRoofHeight(spec.value("roofH", 28))
            .build();
    }
    if (gen == "horizon") {
        return HorizonGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setSkyColor(specColor(spec, "sky", Color(0.12f, 0.12f, 0.16f, 1.0f)))
            .setHillColor(specColor(spec, "hill", Color(0.15f, 0.4f, 0.15f, 1.0f)))
            .setHillCount(spec.value("hillCount", 5))
            .setHillHeight(spec.value("hillMin", 25), spec.value("hillMax", 70))
            .build();
    }
    if (gen == "tree") {
        // Combined trunk + foliage in a single texture (TreeGenerator::build).
        return TreeGenerator().setSize(w, h)
            .setFormat(PixelFormat::ARGB8888).setSeed(seed).setBaseColor(base)
            .setHighlightColor(specColor(spec, "highlight", Color(0.22f, 0.68f, 0.22f, 1.0f)))
            .setDetailColor(specColor(spec, "detail", Color(0.45f, 0.28f, 0.16f, 1.0f)))
            .setTrunkSize(spec.value("trunkW", 14), spec.value("trunkH", 32))
            .setFoliageRadius(spec.value("foliageRadius", 30))
            .build();
    }
    return Material();
}

} // namespace

namespace {

// Flattens a JSON path node's segments into a world-space polygon.
// Quadratic/cubic curves are subdivided into 8 steps each.
void flattenPath(const json& node, std::vector<float>& xs, std::vector<float>& ys) {
    float cx = 0.0f, cy = 0.0f;
    float startX = 0.0f, startY = 0.0f;
    if (!node.contains("segments") || !node["segments"].is_array()) return;
    for (size_t i = 0; i < node["segments"].size(); ++i) {
        const json& seg = node["segments"][i];
        if (!seg.is_array() || seg.empty()) continue;
        const std::string op = seg[0].get<std::string>();
        if (op == "m" && seg.size() >= 3) {
            cx = seg[1].get<float>(); cy = seg[2].get<float>();
            startX = cx; startY = cy;
            xs.push_back(cx); ys.push_back(cy);
        } else if (op == "l" && seg.size() >= 3) {
            cx = seg[1].get<float>(); cy = seg[2].get<float>();
            xs.push_back(cx); ys.push_back(cy);
        } else if (op == "q" && seg.size() >= 5) {
            const float qx = seg[1].get<float>(), qy = seg[2].get<float>();
            const float ex = seg[3].get<float>(), ey = seg[4].get<float>();
            for (int k = 1; k <= 8; ++k) {
                const float t = k / 8.0f, mt = 1.0f - t;
                xs.push_back(mt * mt * cx + 2.0f * mt * t * qx + t * t * ex);
                ys.push_back(mt * mt * cy + 2.0f * mt * t * qy + t * t * ey);
            }
            cx = ex; cy = ey;
        } else if (op == "b" && seg.size() >= 7) {
            const float ax = seg[1].get<float>(), ay = seg[2].get<float>();
            const float bx = seg[3].get<float>(), by = seg[4].get<float>();
            const float ex = seg[5].get<float>(), ey = seg[6].get<float>();
            for (int k = 1; k <= 8; ++k) {
                const float t = k / 8.0f, mt = 1.0f - t;
                xs.push_back(mt * mt * mt * cx + 3.0f * mt * mt * t * ax +
                             3.0f * mt * t * t * bx + t * t * t * ex);
                ys.push_back(mt * mt * mt * cy + 3.0f * mt * mt * t * ay +
                             3.0f * mt * t * t * by + t * t * t * ey);
            }
            cx = ex; cy = ey;
        } else if (op == "z") {
            cx = startX; cy = startY;
        }
    }
}

// Ray-casting point-in-polygon test.
bool pointInPolygon(const std::vector<float>& xs, const std::vector<float>& ys,
                    float px, float py) {
    bool inside = false;
    for (size_t i = 0, j = xs.size() - 1; i < xs.size(); j = i++) {
        if (((ys[i] > py) != (ys[j] > py)) &&
            (px < (xs[j] - xs[i]) * (py - ys[i]) / (ys[j] - ys[i]) + xs[i])) {
            inside = !inside;
        }
    }
    return inside;
}

} // namespace

EditorScene::EditorScene()
    : selNode_(nullptr), selParent_(nullptr), selIndex_(-1) {
    snprintf(pathBuf_, sizeof(pathBuf_), "assets/scenes/game2d.json");
}

void EditorScene::load(domi::World* world, domi::ScriptSystem* script) {
    (void)world; (void)script;
    fitCamera();
    initPalette();
    loadFile(pathBuf_);
}

void EditorScene::unload(domi::World* world, domi::ScriptSystem* script) {
    (void)world; (void)script;
    setRootNode(nullptr);
    clearSelection();
}

void EditorScene::update(double dt) {
    (void)dt;
    // Refresh the texture cache for materials regenerated since last frame.
    // Done here (before render, after the previous frame was flushed) so the
    // re-upload never destroys a texture still referenced by pending draw
    // commands.
    if (dirtyMaterials_.empty()) return;
    domi::Canvas2D* canvas = domi::App::instance().getRender()->getCanvas2D();
    if (canvas) {
        for (size_t i = 0; i < dirtyMaterials_.size(); ++i) {
            std::map<std::string, domi::Material>::const_iterator it =
                materials_.find(dirtyMaterials_[i]);
            if (it == materials_.end()) continue;
            const std::string key = "id:" + it->second.id;
            canvas->uploadMaterial(key.c_str(), it->second);
        }
    }
    dirtyMaterials_.clear();
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
    loadShadowSettings();
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

void EditorScene::exportMaterials(const std::string& jsonPath) {
    // Textures live next to the scene file: <json dir>/../materials/<name>.tex
    std::string dir;
    const size_t slash = jsonPath.find_last_of("/\\");
    if (slash != std::string::npos) dir = jsonPath.substr(0, slash + 1);
    dir += "../materials";
    SDL_CreateDirectory(dir.c_str());

    int written = 0;
    for (std::map<std::string, domi::Material>::const_iterator it = materials_.begin();
         it != materials_.end(); ++it) {
        const std::string path = dir + "/" + it->first + ".tex";
        if (domi::saveMaterialFile(path.c_str(), it->second)) {
            ++written;
        } else {
            fprintf(stderr, "[EDITOR] Failed to write '%s'\n", path.c_str());
        }
    }
    fprintf(stderr, "[EDITOR] Exported %d materials to '%s'\n", written, dir.c_str());
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
        char name[32];
        snprintf(name, sizeof(name), "tree%d", i);
        mats[name] = {{"generator", "tree"}, {"seed", 1000 + i * 137}, {"w", 80}, {"h", 80},
            {"base", {0.15, 0.55, 0.15, 1.0}}, {"highlight", {0.22, 0.68, 0.22, 1.0}},
            {"detail", {0.45, 0.28, 0.16, 1.0}}, {"trunkW", 14}, {"trunkH", 32},
            {"foliageRadius", 30}};
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
    domi::Material m = generateFromSpec(spec);
    if (m.width <= 0) return;
    m.id = name;
    materials_[name] = m;
    dirtyMaterials_.push_back(name);
}

void EditorScene::initPalette() {
    paletteSpecs_["house"] = {{"generator", "house"}, {"seed", 3000}, {"w", 90}, {"h", 90},
        {"base", {0.9, 0.85, 0.6, 1.0}}, {"detail", {0.65, 0.25, 0.2, 1.0}},
        {"wallW", 55}, {"wallH", 45}, {"roofH", 28}};
    paletteSpecs_["rock"] = {{"generator", "rock"}, {"seed", 2000}, {"w", 50}, {"h", 40},
        {"base", {0.55, 0.55, 0.55, 1.0}}, {"rocks", 4}, {"rockRadius", 16}};
    paletteSpecs_["tree"] = {{"generator", "tree"}, {"seed", 1000}, {"w", 80}, {"h", 80},
        {"base", {0.15, 0.55, 0.15, 1.0}}, {"highlight", {0.22, 0.68, 0.22, 1.0}},
        {"detail", {0.45, 0.28, 0.16, 1.0}}, {"trunkW", 14}, {"trunkH", 32},
        {"foliageRadius", 30}};
    paletteSpecs_["cloud"] = {{"generator", "cloud"}, {"seed", 4000}, {"w", 120}, {"h", 80},
        {"base", {0.95, 0.95, 0.98, 1.0}}, {"puffs", 5}, {"puffRadius", 34}};
    for (auto& kv : paletteSpecs_.items()) {
        paletteMats_[kv.key()] = generateFromSpec(kv.value());
    }
}

void EditorScene::panelPalette() {
    static const char* kTypes[] = { "house", "rock", "tree", "cloud" };
    domi::Canvas2D* canvas = domi::App::instance().getRender()->getCanvas2D();
    ImGui::TextDisabled("Drag an image into the preview to place it:");
    for (int i = 0; i < 4; ++i) {
        const char* type = kTypes[i];
        if (i > 0) ImGui::SameLine(0.0f, 28.0f);
        ImGui::PushID(type);
        ImGui::BeginGroup();
        ImGui::Text("%s", type);

        const std::string key = std::string("editor:palette:") + type;
        void* handle = canvas ? canvas->checkMaterial(key.c_str()) : nullptr;
        if (!handle && canvas) handle = canvas->uploadMaterial(key.c_str(), paletteMats_[type]);
        if (handle) {
            // ImGui::Image is a non-interactive item (id 0) and can never be
            // a drag source, so the image is drawn manually over an
            // InvisibleButton that carries the drag.
            const ImVec2 imgPos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##img", ImVec2(72, 72));
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("PALETTE_GEN", type, strlen(type) + 1);
                ImGui::Text("%s", type);
                ImGui::EndDragDropSource();
            }
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)handle, imgPos, ImVec2(imgPos.x + 72, imgPos.y + 72));
        }

        json& spec = paletteSpecs_[type];
        ImGui::PushItemWidth(150.0f);
        bool changed = false;
        changed |= dragInt("seed", spec, "seed");
        changed |= dragInt("w", spec, "w");
        changed |= dragInt("h", spec, "h");
        changed |= colorEdit("base", spec, "base");
        const std::string gen = spec.value("generator", "");
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
        } else if (gen == "tree") {
            changed |= colorEdit("highlight", spec, "highlight");
            changed |= colorEdit("detail", spec, "detail");
            changed |= dragInt("trunkW", spec, "trunkW");
            changed |= dragInt("trunkH", spec, "trunkH");
            changed |= dragInt("foliageRadius", spec, "foliageRadius");
        }
        ImGui::PopItemWidth();
        if (changed) {
            paletteMats_[type] = generateFromSpec(spec);
            if (canvas) canvas->uploadMaterial(key.c_str(), paletteMats_[type]);
        }
        ImGui::EndGroup();
        ImGui::PopID();
    }
}

void EditorScene::dropPaletteMaterial(const std::string& genType, float wx, float wy) {
    if (!paletteSpecs_.contains(genType)) return;
    if (!doc_.contains("materials") || !doc_["materials"].is_object()) {
        doc_["materials"] = json::object();
    }

    // Unique material name.
    std::string name;
    for (int i = 1;; ++i) {
        name = genType + "_" + std::to_string(i);
        if (!doc_["materials"].contains(name) && !materials_.count(name)) break;
    }

    // The doc_ mutations below may reallocate node arrays, which would leave
    // selNode_/dragNode_ dangling.
    clearSelection();

    const json spec = paletteSpecs_[genType];
    doc_["materials"][name] = spec;
    generateMaterial(name, spec);

    // Add a node for it to the object layer.
    const json node = {{"type", "material"}, {"id", name}, {"material", name},
                       {"x", wx}, {"y", wy}, {"centered", true},
                       {"sort", "bottom"}, {"castShadow", true}};
    bool added = false;
    if (doc_.contains("root") && doc_["root"].contains("children")) {
        for (auto& child : doc_["root"]["children"]) {
            if (child.value("type", "") == "layer" &&
                child.value("layer", "") == "object" &&
                child.contains("children")) {
                child["children"].push_back(node);
                added = true;
                break;
            }
        }
    }
    if (!added) {
        fprintf(stderr, "[EDITOR] No object layer found, node not added\n");
    }
    rebuildTree();
    fprintf(stderr, "[EDITOR] Placed '%s' at (%.0f, %.0f)\n", name.c_str(), wx, wy);
}

void EditorScene::loadShadowSettings() {
    shadowEnabled_ = doc_.value("shadowEnabled", true);
    shadowLightDir_ = domi::Vec2(0.4f, -0.7f);
    if (doc_.contains("shadowLightDir") && doc_["shadowLightDir"].is_array() &&
        doc_["shadowLightDir"].size() >= 2) {
        shadowLightDir_.x = doc_["shadowLightDir"][0].get<float>();
        shadowLightDir_.y = doc_["shadowLightDir"][1].get<float>();
    }
}

void EditorScene::saveShadowSettings() {
    doc_["shadowEnabled"] = shadowEnabled_;
    doc_["shadowLightDir"] = { shadowLightDir_.x, shadowLightDir_.y };
}

// static
bool EditorScene::findMaterialJson(nlohmann::json& node,
                                   const std::string& material,
                                   float x, float y,
                                   nlohmann::json** out) {
    const std::string type = node.value("type", "");
    if (type == "material" && node.value("material", "") == material) {
        const float nx = node.value("x", 0.0f);
        const float ny = node.value("y", 0.0f);
        if (std::abs(nx - x) < 0.001f && std::abs(ny - y) < 0.001f) {
            *out = &node;
            return true;
        }
    }
    if (node.contains("children") && node["children"].is_array()) {
        json& children = node["children"];
        for (size_t i = 0; i < children.size(); ++i) {
            if (findMaterialJson(children[i], material, x, y, out)) return true;
        }
    }
    return false;
}

void EditorScene::refreshShadowCasters(domi::SceneLoader& loader) {
    shadowCasters_.clear();
    if (!shadowEnabled_) return;

    const std::vector<std::pair<std::string, domi::MaterialNode*> >& mats =
        loader.materialNodes();
    for (size_t i = 0; i < mats.size(); ++i) {
        domi::MaterialNode* node = mats[i].second;
        if (!node || !node->getCastShadow()) continue;
        json* j = nullptr;
        findMaterialJson(doc_["root"], mats[i].first, node->getX(), node->getY(), &j);
        ShadowCaster caster;
        caster.name = mats[i].first;
        caster.node = node;
        caster.jsonNode = j;
        shadowCasters_.push_back(caster);
    }
}

void EditorScene::drawEditorShadows(domi::DrawBatch& batch) const {
    if (!shadowEnabled_ || shadowCasters_.empty()) return;

    domi::Vec2 shadowDir = shadowLightDir_ * -1.0f;
    shadowDir = shadowDir.normalized();
    if (shadowDir.y <= 0.0f) return;

    for (size_t i = 0; i < shadowCasters_.size(); ++i) {
        const domi::MaterialNode* node = shadowCasters_[i].node;
        const domi::Material* mat = node->getMaterial();
        if (!mat || mat->width <= 0) continue;
        const float s = node->getScale();
        const float w = static_cast<float>(mat->width) * s;
        const float h = static_cast<float>(mat->height);
        const float cx = node->isCentered() ? node->getX() : node->getX() + w * 0.5f;
        const float cy = node->isCentered() ? node->getY() + h * 0.5f
                                            : node->getY() + h;
        domi::SceneFunction::drawShadow(batch, cx, cy,
                                        w * 0.55f, w * 0.18f, h * 0.5f,
                                        shadowDir);
    }
}

void EditorScene::rebuildTree() {
    using namespace domi;
    SceneLoader loader;
    loader.setMaterialResolver([this](const std::string& name) -> const Material* {
        std::map<std::string, Material>::iterator it = materials_.find(name);
        return it != materials_.end() ? &it->second : nullptr;
    });
    std::unique_ptr<GroupNode> root = loader.loadFromJson(doc_);
    refreshShadowCasters(loader);
    if (root) {
        root->shadowLayer().addChild<CustomNode>(
            [this](DrawBatch& batch) { drawEditorShadows(batch); });
    }
    setRootNode(std::move(root));
}

void EditorScene::fitCamera() {
    // The preview lives right of the (adjustable) left panel and above the
    // (adjustable) bottom palette. Fit the whole world into that rect.
    domi::RenderSystem* render = domi::App::instance().getRender();
    const float winW = static_cast<float>(render->getWidth());
    const float winH = static_cast<float>(render->getHeight());
    previewX_ = panelWidth_ + 10.0f;
    previewY_ = navH_;
    previewW_ = winW - previewX_;
    previewH_ = winH - navH_ - paletteHeight_ - 8.0f;
    if (previewW_ < 100.0f) previewW_ = 100.0f;
    if (previewH_ < 100.0f) previewH_ = 100.0f;

    const float kWorldW = 2560.0f;
    const float kWorldH = 1440.0f;
    // Cover: fill the preview rect completely and crop the overflow (no
    // letterbox bands), instead of fitting the whole world inside it.
    float zoom = previewW_ / kWorldW;
    if (previewH_ / kWorldH > zoom) zoom = previewH_ / kWorldH;
    fitZoom_ = zoom;
    camera_.zoom = zoom;
    camera_.offsetX = previewX_ + (previewW_ - kWorldW * zoom) * 0.5f;
    camera_.offsetY = previewY_ + (previewH_ - kWorldH * zoom) * 0.5f;
    camera_.clip = true;
    camera_.clipX = previewX_;
    camera_.clipY = previewY_;
    camera_.clipW = previewW_;
    camera_.clipH = previewH_;

    layoutWinW_ = winW;
    layoutWinH_ = winH;
    layoutNavH_ = navH_;
    layoutPanelW_ = panelWidth_;
    layoutPaletteH_ = paletteHeight_;
}

void EditorScene::updatePreviewLayout() {
    domi::RenderSystem* render = domi::App::instance().getRender();
    const float winW = static_cast<float>(render->getWidth());
    const float winH = static_cast<float>(render->getHeight());
    if (winW != layoutWinW_ || winH != layoutWinH_ || navH_ != layoutNavH_ ||
        panelWidth_ != layoutPanelW_ || paletteHeight_ != layoutPaletteH_) {
        // Layout changed: refit the camera into the new preview rect.
        fitCamera();
    } else {
        previewX_ = panelWidth_ + 10.0f;
        previewY_ = navH_;
        previewW_ = winW - previewX_;
        previewH_ = winH - navH_ - paletteHeight_ - 8.0f;
    }
}

bool EditorScene::handleSplitters() {
    ImGuiIO& io = ImGui::GetIO();
    domi::RenderSystem* render = domi::App::instance().getRender();
    const float winW = static_cast<float>(render->getWidth());
    const float winH = static_cast<float>(render->getHeight());
    const float mx = io.MousePos.x;
    const float my = io.MousePos.y;

    // Vertical splitter: the gap between the left panel and the preview.
    const bool onV = !dragSplitH_ &&
        mx >= panelWidth_ && mx <= panelWidth_ + 10.0f && my >= navH_ && my <= winH;
    // Horizontal splitter: the gap between the preview and the palette.
    const float splitHY = winH - paletteHeight_ - 8.0f;
    const bool onH = !dragSplitV_ &&
        my >= splitHY && my <= splitHY + 8.0f && mx >= previewX_;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (onV) dragSplitV_ = true;
        else if (onH) dragSplitH_ = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragSplitV_ = false;
        dragSplitH_ = false;
    }

    if (dragSplitV_) {
        panelWidth_ = mx;
        if (panelWidth_ < 200.0f) panelWidth_ = 200.0f;
        if (panelWidth_ > winW - 300.0f) panelWidth_ = winW - 300.0f;
    } else if (dragSplitH_) {
        paletteHeight_ = winH - my;
        if (paletteHeight_ < 80.0f) paletteHeight_ = 80.0f;
        if (paletteHeight_ > winH - 200.0f) paletteHeight_ = winH - 200.0f;
    }

    if (dragSplitV_ || onV) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    else if (dragSplitH_ || onH) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

    // Splitter visuals (brightened on hover/drag).
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 vcol = (dragSplitV_ || onV) ? IM_COL32(120, 160, 220, 255)
                                            : IM_COL32(70, 70, 70, 255);
    const ImU32 hcol = (dragSplitH_ || onH) ? IM_COL32(120, 160, 220, 255)
                                            : IM_COL32(70, 70, 70, 255);
    dl->AddRectFilled(ImVec2(panelWidth_ + 4.0f, navH_),
                      ImVec2(panelWidth_ + 6.0f, winH), vcol);
    dl->AddRectFilled(ImVec2(previewX_, splitHY + 3.0f),
                      ImVec2(previewX_ + previewW_, splitHY + 5.0f), hcol);

    return dragSplitV_ || dragSplitH_;
}

void EditorScene::clampCamera() {
    // Keep the preview rect inside the world; center the axis that doesn't
    // fill the rect at the current zoom.
    const float kWorldW = 2560.0f;
    const float kWorldH = 1440.0f;
    const float viewW = kWorldW * camera_.zoom;
    const float viewH = kWorldH * camera_.zoom;

    if (viewW >= previewW_) {
        if (camera_.offsetX > previewX_) camera_.offsetX = previewX_;
        if (camera_.offsetX < previewX_ + previewW_ - viewW)
            camera_.offsetX = previewX_ + previewW_ - viewW;
    } else {
        camera_.offsetX = previewX_ + (previewW_ - viewW) * 0.5f;
    }

    if (viewH >= previewH_) {
        if (camera_.offsetY > previewY_) camera_.offsetY = previewY_;
        if (camera_.offsetY < previewY_ + previewH_ - viewH)
            camera_.offsetY = previewY_ + previewH_ - viewH;
    } else {
        camera_.offsetY = previewY_ + (previewH_ - viewH) * 0.5f;
    }
}

void EditorScene::worldToScreen(float wx, float wy, float& sx, float& sy) const {
    sx = wx * camera_.zoom + camera_.offsetX;
    sy = wy * camera_.zoom + camera_.offsetY;
}

void EditorScene::screenToWorld(float sx, float sy, float& wx, float& wy) const {
    wx = (sx - camera_.offsetX) / camera_.zoom;
    wy = (sy - camera_.offsetY) / camera_.zoom;
}

bool EditorScene::nodeBounds(const json& node,
                             float& x0, float& y0, float& x1, float& y1) const {
    const std::string type = node.value("type", "");

    if (type == "group" || type == "layer") {
        bool any = false;
        x0 = y0 = 1e30f;
        x1 = y1 = -1e30f;
        if (node.contains("children") && node["children"].is_array()) {
            for (size_t i = 0; i < node["children"].size(); ++i) {
                float cx0, cy0, cx1, cy1;
                if (nodeBounds(node["children"][i], cx0, cy0, cx1, cy1)) {
                    if (cx0 < x0) x0 = cx0;
                    if (cy0 < y0) y0 = cy0;
                    if (cx1 > x1) x1 = cx1;
                    if (cy1 > y1) y1 = cy1;
                    any = true;
                }
            }
        }
        return any;
    }

    if (type == "rect") {
        x0 = node.value("x", 0.0f);
        y0 = node.value("y", 0.0f);
        x1 = x0 + node.value("w", 0.0f);
        y1 = y0 + node.value("h", 0.0f);
        return true;
    }
    if (type == "ellipse") {
        const float x = node.value("x", 0.0f);
        const float y = node.value("y", 0.0f);
        const float rx = node.value("rx", 0.0f);
        const float ry = node.value("ry", 0.0f);
        x0 = x - rx; y0 = y - ry; x1 = x + rx; y1 = y + ry;
        return true;
    }
    if (type == "line") {
        const float ax = node.value("x1", 0.0f);
        const float ay = node.value("y1", 0.0f);
        const float bx = node.value("x2", 0.0f);
        const float by = node.value("y2", 0.0f);
        x0 = ax < bx ? ax : bx;
        y0 = ay < by ? ay : by;
        x1 = ax > bx ? ax : bx;
        y1 = ay > by ? ay : by;
        return true;
    }
    if (type == "path") {
        std::vector<float> xs, ys;
        flattenPath(node, xs, ys);
        if (xs.empty()) return false;
        x0 = y0 = 1e30f;
        x1 = y1 = -1e30f;
        for (size_t i = 0; i < xs.size(); ++i) {
            if (xs[i] < x0) x0 = xs[i];
            if (ys[i] < y0) y0 = ys[i];
            if (xs[i] > x1) x1 = xs[i];
            if (ys[i] > y1) y1 = ys[i];
        }
        return true;
    }
    if (type == "material" || type == "cloud") {
        std::map<std::string, domi::Material>::const_iterator it =
            materials_.find(node.value("material", ""));
        if (it == materials_.end()) return false;
        const float w = static_cast<float>(it->second.width);
        const float h = static_cast<float>(it->second.height);
        const float x = node.value("x", 0.0f);
        const float y = node.value("y", 0.0f);
        // Clouds are always built with centered=true.
        const bool centered = (type == "cloud") || node.value("centered", false);
        x0 = centered ? x - w * 0.5f : x;
        y0 = centered ? y - h * 0.5f : y;
        x1 = x0 + w;
        y1 = y0 + h;
        return true;
    }
    return false;
}

void EditorScene::hitTest(json& node, json& parent, int index,
                          float wx, float wy,
                          json** hit, json** hitParent, int* hitIndex) {
    // Children draw on top of their parent; test them first, in reverse
    // order so the last-drawn (topmost) child wins.
    if (node.contains("children") && node["children"].is_array()) {
        json& children = node["children"];
        for (int i = static_cast<int>(children.size()) - 1; i >= 0; --i) {
            hitTest(children[i], children, i, wx, wy, hit, hitParent, hitIndex);
            if (*hit) return;
        }
    }
    const std::string type = node.value("type", "");
    if (type == "group" || type == "layer") return;
    float x0, y0, x1, y1;
    if (!nodeBounds(node, x0, y0, x1, y1) ||
        wx < x0 || wx > x1 || wy < y0 || wy > y1) return;
    // Paths use an exact point-in-polygon test instead of the bounding box
    // (the road's bbox would otherwise swallow clicks meant for the lake).
    if (type == "path") {
        std::vector<float> xs, ys;
        flattenPath(node, xs, ys);
        if (!pointInPolygon(xs, ys, wx, wy)) return;
    }
    *hit = &node;
    *hitParent = &parent;
    *hitIndex = index;
}

void EditorScene::handleCameraMouse() {
    ImGuiIO& io = ImGui::GetIO();
    const bool inPreview =
        io.MousePos.x >= previewX_ && io.MousePos.x <= previewX_ + previewW_ &&
        io.MousePos.y >= previewY_ && io.MousePos.y <= previewY_ + previewH_;

    // Left-drag pans (once started inside the preview, the drag continues
    // even if the cursor leaves it).
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        (draggingCamera_ || (inPreview && !io.WantCaptureMouse &&
                             ImGui::IsMouseDragging(ImGuiMouseButton_Left)))) {
        draggingCamera_ = true;
        camera_.offsetX += io.MouseDelta.x;
        camera_.offsetY += io.MouseDelta.y;
    } else {
        draggingCamera_ = false;
    }

    // Wheel zooms toward the cursor; never zoom out past the fit view.
    if (inPreview && io.MouseWheel != 0.0f) {
        const float oldZoom = camera_.zoom;
        float newZoom = oldZoom * (io.MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f);
        if (newZoom < fitZoom_) newZoom = fitZoom_;
        if (newZoom > 4.0f) newZoom = 4.0f;
        const float mx = io.MousePos.x;
        const float my = io.MousePos.y;
        camera_.offsetX = mx - (mx - camera_.offsetX) * (newZoom / oldZoom);
        camera_.offsetY = my - (my - camera_.offsetY) * (newZoom / oldZoom);
        camera_.zoom = newZoom;
    }

    clampCamera();
}

void EditorScene::handlePreviewMouse() {
    if (!doc_.contains("root")) return;
    if (cameraMode_) {
        handleCameraMouse();
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    float wx, wy;
    screenToWorld(io.MousePos.x, io.MousePos.y, wx, wy);

    if (dragNode_) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const float dx = wx - dragLastX_;
            const float dy = wy - dragLastY_;
            dragLastX_ = wx;
            dragLastY_ = wy;
            if (dx != 0.0f || dy != 0.0f) {
                translateNode(*dragNode_, dx, dy);
                rebuildTree();
            }
        } else {
            dragNode_ = nullptr;
        }
        return;
    }

    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || io.WantCaptureMouse) return;

    json* hit = nullptr;
    json* hitParent = nullptr;
    int hitIndex = -1;
    hitTest(doc_["root"], doc_["root"], -1, wx, wy, &hit, &hitParent, &hitIndex);
    if (!hit) {
        clearSelection();
        return;
    }
    selectNode(*hit, *hitParent, hitIndex);

    // Nodes positioned by x/y, and paths (translated per segment), are draggable.
    const std::string type = hit->value("type", "");
    if (type == "material" || type == "cloud" || type == "tree" ||
        type == "rect" || type == "ellipse" || type == "path" || type == "line") {
        dragNode_ = hit;
        dragLastX_ = wx;
        dragLastY_ = wy;
    }
}

// static
void EditorScene::translateNode(json& node, float dx, float dy) {
    const std::string type = node.value("type", "");
    if (type == "path") {
        if (!node.contains("segments") || !node["segments"].is_array()) return;
        for (size_t i = 0; i < node["segments"].size(); ++i) {
            json& seg = node["segments"][i];
            if (!seg.is_array() || seg.empty() || !seg[0].is_string()) continue;
            if (seg[0].get<std::string>() == "z") continue;
            // Coordinate pairs start at index 1 for m/l/q/b ops.
            for (size_t k = 1; k + 1 < seg.size(); k += 2) {
                seg[k] = seg[k].get<float>() + dx;
                seg[k + 1] = seg[k + 1].get<float>() + dy;
            }
        }
    } else if (type == "line") {
        node["x1"] = node.value("x1", 0.0f) + dx;
        node["y1"] = node.value("y1", 0.0f) + dy;
        node["x2"] = node.value("x2", 0.0f) + dx;
        node["y2"] = node.value("y2", 0.0f) + dy;
    } else {
        node["x"] = node.value("x", 0.0f) + dx;
        node["y"] = node.value("y", 0.0f) + dy;
    }
}

void EditorScene::drawSelectionHighlight() const {
    if (!selNode_) return;
    float x0, y0, x1, y1;
    if (!nodeBounds(*selNode_, x0, y0, x1, y1)) return;
    float sx0, sy0, sx1, sy1;
    worldToScreen(x0, y0, sx0, sy0);
    worldToScreen(x1, y1, sx1, sy1);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRect(ImVec2(sx0, sy0), ImVec2(sx1, sy1),
                IM_COL32(255, 200, 40, 255), 0.0f, 0, 2.0f);
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
    dragNode_ = nullptr;
    propsLastSel_ = nullptr;
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
        bool cast = n.value("castShadow", true);
        if (ImGui::Checkbox("cast shadow", &cast)) {
            n["castShadow"] = cast;
            changed = true;
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
    if (materials_.count(selectedMaterial_)) {
        previewMaterial(canvas, selectedMaterial_, materials_[selectedMaterial_]);
    }

    if (changed || ImGui::Button("Regenerate")) {
        generateMaterial(selectedMaterial_, spec);
        rebuildTree();
    }
}

void EditorScene::panelShadow() {
    bool enabled = shadowEnabled_;
    if (ImGui::Checkbox("Shadows enabled", &enabled)) {
        shadowEnabled_ = enabled;
        saveShadowSettings();
        rebuildTree();
    }

    float dir[2] = { shadowLightDir_.x, shadowLightDir_.y };
    if (ImGui::InputFloat2("Light direction", dir)) {
        shadowLightDir_.x = dir[0];
        shadowLightDir_.y = dir[1];
        saveShadowSettings();
    }

    ImGui::Separator();
    ImGui::Text("Shadow casters:");
    if (shadowCasters_.empty()) {
        ImGui::TextDisabled("No casters. Enable 'cast shadow' on a material node.");
        return;
    }
    for (size_t i = 0; i < shadowCasters_.size(); ++i) {
        ShadowCaster& caster = shadowCasters_[i];
        bool cast = true;
        if (caster.jsonNode) {
            cast = caster.jsonNode->value("castShadow", true);
        }
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Checkbox(caster.name.c_str(), &cast)) {
            if (caster.jsonNode) {
                (*caster.jsonNode)["castShadow"] = cast;
                rebuildTree();
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Select")) {
            if (caster.jsonNode) {
                selNode_ = caster.jsonNode;
                selParent_ = nullptr;
                selIndex_ = -1;
                propsLastSel_ = nullptr;
            }
        }
        ImGui::PopID();
    }
}

void EditorScene::drawNodeProperties() {
    if (!selNode_) {
        propsLastSel_ = nullptr;
        return;
    }
    float x0, y0, x1, y1;
    if (!nodeBounds(*selNode_, x0, y0, x1, y1)) return;

    // Anchor the window to the object's top-right (flipped to the left side
    // when it would leave the window).
    domi::RenderSystem* render = domi::App::instance().getRender();
    const float winW = static_cast<float>(render->getWidth());
    const float winH = static_cast<float>(render->getHeight());
    float sx0, sy0, sx1, sy1;
    worldToScreen(x0, y0, sx0, sy0);
    worldToScreen(x1, y1, sx1, sy1);
    float posX = sx1 + 8.0f;
    if (posX + 260.0f > winW) posX = sx0 - 268.0f;
    if (posX < previewX_) posX = previewX_ + 4.0f;
    float posY = sy0;
    if (posY > winH - 220.0f) posY = winH - 220.0f;
    if (posY < navH_) posY = navH_;

    if (propsCollapsed_) {
        ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
        ImGui::Begin("##props_collapsed", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
        if (ImGui::SmallButton("Properties >>")) propsCollapsed_ = false;
        ImGui::End();
        return;
    }

    // Reposition when the selection changes; afterwards the user may drag
    // the window freely.
    if (selNode_ != propsLastSel_) {
        propsLastSel_ = selNode_;
        ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    }
    ImGui::Begin("Node Properties", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
    if (ImGui::SmallButton("Collapse <<")) propsCollapsed_ = true;
    ImGui::Separator();
    panelProperties();
    ImGui::End();
}

void EditorScene::drawEditor() {
    domi::RenderSystem* render = domi::App::instance().getRender();
    const float winW = static_cast<float>(render->getWidth());
    const float winH = static_cast<float>(render->getHeight());

    // Window-wide navigation bar at the very top.
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            ImGui::Text("Scene file:");
            ImGui::PushItemWidth(220.0f);
            ImGui::InputText("##scenepath", pathBuf_, sizeof(pathBuf_));
            ImGui::PopItemWidth();
            if (ImGui::MenuItem("Import")) {
                loadFile(pathBuf_);
            }
            if (ImGui::MenuItem("Save")) {
                const std::string target = filePath_.empty() ? pathBuf_ : filePath_;
                saveFile(target);
                exportMaterials(target);
            }
            if (ImGui::MenuItem("Save As")) {
                saveFile(pathBuf_);
                exportMaterials(pathBuf_);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Textures")) {
                exportMaterials(filePath_.empty() ? pathBuf_ : filePath_);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Shadow")) {
            panelShadow();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    navH_ = ImGui::GetFrameHeight();

    updatePreviewLayout();

    ImGui::SetNextWindowPos(ImVec2(0, navH_), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth_, winH - navH_), ImGuiCond_Always);
    ImGui::Begin("Domi Editor", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    // Mouse interaction mode for the preview area.
    int mode = cameraMode_ ? 1 : 0;
    ImGui::Text("Mode:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Design", &mode, 0)) cameraMode_ = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("Camera", &mode, 1)) cameraMode_ = true;
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) panelHierarchy();
    if (ImGui::CollapsingHeader("Materials")) panelMaterials();
    ImGui::End();

    // Generator palette at the bottom-right, below the preview.
    ImGui::SetNextWindowPos(ImVec2(previewX_, winH - paletteHeight_), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(previewW_, paletteHeight_), ImGuiCond_Always);
    ImGui::Begin("Generators", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    panelPalette();
    ImGui::End();

    // Accept palette drops into the preview area (it is not an ImGui window,
    // so the payload is checked manually on mouse release).
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (payload && strcmp(payload->DataType, "PALETTE_GEN") == 0 &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.MousePos.x >= previewX_ && io.MousePos.x <= previewX_ + previewW_ &&
            io.MousePos.y >= previewY_ && io.MousePos.y <= previewY_ + previewH_) {
            float wx, wy;
            screenToWorld(io.MousePos.x, io.MousePos.y, wx, wy);
            dropPaletteMaterial(static_cast<const char*>(payload->Data), wx, wy);
        }
    }

    // Splitters between the panels (draggable panel borders).
    const bool splitterActive = handleSplitters();

    // Outline the preview area so its bounds are visible.
    ImGui::GetBackgroundDrawList()->AddRect(
        ImVec2(previewX_, previewY_),
        ImVec2(previewX_ + previewW_, previewY_ + previewH_),
        IM_COL32(90, 90, 90, 255));

    if (!splitterActive) handlePreviewMouse();
    drawSelectionHighlight();
    drawNodeProperties();
}
