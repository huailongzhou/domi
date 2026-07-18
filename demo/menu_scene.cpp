#include "menu_scene.h"

#include "domi/app.h"
#include "domi/input.h"
#include "domi/scene_manager.h"
#include "domi/render_node.h"
#include "domi/ui/clay_ui.h"
#include "game2d_scene.h"
#include "game3d_scene.h"
#include <cstdio>

using namespace domi;

namespace {

// One centered menu button.
void menuButton(ClayUI& ui, const char* id, const char* label,
                const Color& background, const Color& hoveredBg,
                const Color& border) {
    ClayUI::Box box;
    box.id = id;
    box.width = 400.0f;
    box.height = 70.0f;
    box.background = ui.hovered(id) ? hoveredBg : background;
    box.borderColor = border;
    box.borderWidth = 4;
    box.cornerRadius = 10.0f;
    ui.beginBox(box);
    ui.text(label, 24, Color(1.0f, 1.0f, 1.0f, 1.0f));
    ui.endBox();
}

} // namespace

MenuScene::MenuScene() : pendingChoice_(0) {}

void MenuScene::load(World* world, ScriptSystem* script) {
    (void)world;
    (void)script;

    std::unique_ptr<GroupNode> root(new GroupNode());
    LayerView& background = root->backgroundLayer();
    // Sorted by top edge: background (0), title bar (160), hint bars (560+).
    background.addChild<RectNode>(
        0.0f, 0.0f, 1280.0f, 720.0f, Color(0.08f, 0.08f, 0.12f)).sortByTop();
    background.addChild<RectNode>(
        340.0f, 160.0f, 600.0f, 80.0f, Color(0.9f, 0.9f, 0.9f)).sortByTop();
    background.addChild<RectNode>(
        440.0f, 560.0f, 400.0f, 6.0f, Color(0.7f, 0.7f, 0.7f)).sortByTop();
    background.addChild<RectNode>(
        440.0f, 580.0f, 400.0f, 6.0f, Color(0.7f, 0.7f, 0.7f)).sortByTop();
    background.addChild<RectNode>(
        440.0f, 600.0f, 280.0f, 6.0f, Color(0.7f, 0.7f, 0.7f)).sortByTop();
    setRootNode(std::move(root));
}

void MenuScene::unload(World* world, ScriptSystem* script) {
    (void)world;
    (void)script;
    setRootNode(nullptr);
}

bool MenuScene::buildClayUI(ClayUI& ui) {
    ClayUI::Box root;
    root.width = 1280.0f;
    root.height = 720.0f;
    root.paddingT = 120; // nudge the stack below the title bar
    root.childGap = 20;
    ui.beginBox(root);
    menuButton(ui, "MENU_2D", "2D Game",
               Color(0.25f, 0.65f, 0.25f), Color(0.35f, 0.78f, 0.35f),
               Color(0.15f, 0.45f, 0.15f));
    menuButton(ui, "MENU_3D", "3D Game",
               Color(0.25f, 0.45f, 0.75f), Color(0.35f, 0.57f, 0.86f),
               Color(0.15f, 0.30f, 0.55f));
    ui.endBox();

    if (ui.clicked("MENU_2D")) pendingChoice_ = 2;
    if (ui.clicked("MENU_3D")) pendingChoice_ = 3;
    return true;
}

void MenuScene::update(double dt) {
    (void)dt;
    InputSystem* input = App::instance().getInput();

    bool choose2D = pendingChoice_ == 2;
    bool choose3D = pendingChoice_ == 3;
    pendingChoice_ = 0;

    if (input) {
        choose2D = choose2D || input->isKeyPressed(SDL_SCANCODE_1) ||
                   input->isKeyPressed(SDL_SCANCODE_KP_1);
        choose3D = choose3D || input->isKeyPressed(SDL_SCANCODE_2) ||
                   input->isKeyPressed(SDL_SCANCODE_KP_2);
    }

    if (choose2D) {
        fprintf(stderr, "[MENU] Starting 2D game\n");
        App::instance().getSceneManager()->setNext(new Game2DScene());
    } else if (choose3D) {
        fprintf(stderr, "[MENU] Starting 3D game\n");
        App::instance().getSceneManager()->setNext(new Game3DScene());
    }
}
