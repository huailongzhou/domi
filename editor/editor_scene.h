#ifndef EDITOR_SCENE_H
#define EDITOR_SCENE_H

#include "domi/scene.h"
#include "domi/scene_loader.h"
#include "domi/material.h"
#include <map>
#include <string>

// A minimal scene editor: loads a scene JSON file (node tree + a
// "materials" section describing procedural texture generators),
// renders a live preview through the normal render pipeline, and lets the
// user edit nodes and material generator parameters via ImGui panels.
class EditorScene : public domi::Scene {
public:
    EditorScene();

    const char* name() const override { return "EditorScene"; }

    void load(domi::World* world, domi::ScriptSystem* script) override;
    void unload(domi::World* world, domi::ScriptSystem* script) override;
    void update(double dt) override;

    // Called by the pre-present hook with an active ImGui frame.
    void drawEditor();

private:
    // The whole scene document: {"comment", "materials", "root"}.
    nlohmann::json doc_;
    std::string filePath_;
    char pathBuf_[256];

    // Materials generated from doc_["materials"].
    std::map<std::string, domi::Material> materials_;
    std::string selectedMaterial_;

    // Node selection: pointer into doc_ plus its parent array and index.
    nlohmann::json* selNode_;
    nlohmann::json* selParent_;
    int selIndex_;

    void loadFile(const std::string& path);
    void saveFile(const std::string& path);
    void ensureMaterialsSection();

    // Material generation from a spec in doc_["materials"].
    void generateMaterials();
    void generateMaterial(const std::string& name, const nlohmann::json& spec);

    // Rebuild the render node tree from doc_.
    void rebuildTree();

    // ImGui panels.
    void panelFiles();
    void panelHierarchy();
    void panelProperties();
    void panelMaterials();
    void hierarchyChildren(nlohmann::json& node);
    std::string nodeLabel(const nlohmann::json& node, int index) const;
    void selectNode(nlohmann::json& node, nlohmann::json& parent, int index);
    void clearSelection();
};

#endif // EDITOR_SCENE_H
