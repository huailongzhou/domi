#ifndef EDITOR_SCENE_H
#define EDITOR_SCENE_H

#include "domi/scene.h"
#include "domi/scene_loader.h"
#include "domi/material.h"
#include "domi/camera2d.h"
#include <map>
#include <string>
#include <vector>

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

    // The editor previews the whole world fitted into the window.
    const domi::Camera2D* getCamera2D() const override { return &camera_; }

private:
    // The whole scene document: {"comment", "materials", "root"}.
    nlohmann::json doc_;
    std::string filePath_;
    char pathBuf_[256];

    // Materials generated from doc_["materials"].
    std::map<std::string, domi::Material> materials_;
    std::string selectedMaterial_;

    // Ids of materials regenerated since the last frame; update() re-uploads
    // them so the id-keyed texture cache picks up the new pixels.
    std::vector<std::string> dirtyMaterials_;

    // Bottom-right generator palette: per-generator specs and their preview
    // materials; dragging one into the preview creates a scene material node.
    nlohmann::json paletteSpecs_;
    std::map<std::string, domi::Material> paletteMats_;

    // Node selection: pointer into doc_ plus its parent array and index.
    nlohmann::json* selNode_;
    nlohmann::json* selParent_;
    int selIndex_;

    // Preview camera: fitted into the preview rect (top-right of the window)
    // in load(); panned/zoomed in camera mode.
    domi::Camera2D camera_;

    // Preview rect in screen coordinates and the zoom that fits the whole
    // world into it (the minimum zoom in camera mode).
    float previewX_ = 0.0f;
    float previewY_ = 0.0f;
    float previewW_ = 0.0f;
    float previewH_ = 0.0f;
    float fitZoom_ = 1.0f;

    // Mouse interaction mode: design = select/drag elements,
    // camera = pan/zoom the viewport.
    bool cameraMode_ = false;
    bool draggingCamera_ = false;

    // Mouse dragging in the preview: the JSON node being moved and the last
    // mouse world position (movement is applied as per-frame deltas so path
    // nodes can be translated too).
    nlohmann::json* dragNode_ = nullptr;
    float dragLastX_ = 0.0f;
    float dragLastY_ = 0.0f;

    // Floating properties window state.
    bool propsCollapsed_ = false;
    nlohmann::json* propsLastSel_ = nullptr;

    void loadFile(const std::string& path);
    void saveFile(const std::string& path);
    // Writes all generated materials as raw .tex files next to the scene
    // file (<json dir>/../materials/<name>.tex) for the game to load.
    void exportMaterials(const std::string& jsonPath);
    void ensureMaterialsSection();

    // Material generation from a spec in doc_["materials"].
    void generateMaterials();
    void generateMaterial(const std::string& name, const nlohmann::json& spec);

    // Rebuild the render node tree from doc_.
    void rebuildTree();

    // Preview interaction helpers (called from drawEditor).
    void fitCamera();
    void clampCamera();
    void handlePreviewMouse();
    void handleCameraMouse();
    void drawSelectionHighlight() const;
    bool nodeBounds(const nlohmann::json& node,
                    float& x0, float& y0, float& x1, float& y1) const;
    void hitTest(nlohmann::json& node, nlohmann::json& parent, int index,
                 float wx, float wy,
                 nlohmann::json** hit, nlohmann::json** hitParent, int* hitIndex);
    static void translateNode(nlohmann::json& node, float dx, float dy);
    void worldToScreen(float wx, float wy, float& sx, float& sy) const;
    void screenToWorld(float sx, float sy, float& wx, float& wy) const;

    // ImGui panels.
    void panelFiles();
    void panelHierarchy();
    void panelProperties();
    void panelMaterials();
    void panelPalette();
    void initPalette();
    void dropPaletteMaterial(const std::string& genType, float wx, float wy);
    // Floating properties window shown next to the selected object.
    void drawNodeProperties();
    void hierarchyChildren(nlohmann::json& node);
    std::string nodeLabel(const nlohmann::json& node, int index) const;
    void selectNode(nlohmann::json& node, nlohmann::json& parent, int index);
    void clearSelection();
};

#endif // EDITOR_SCENE_H
