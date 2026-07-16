#include "menu_scene.h"

#include "domi/app.h"
#include "domi/input.h"
#include "domi/scene_manager.h"
#include "domi/render_node.h"
#include "game2d_scene.h"
#include "game3d_scene.h"
#include <cstdio>

using namespace domi;

MenuScene::MenuScene() {
    // SwiftUI-like declarative menu layout:
    // a centered vertical stack of two rounded buttons.
    menu_ = UIView::VStack(20.0f, {
        UIView::Button("2D Game")
            .onClick([]() {
                fprintf(stderr, "[MENU] Starting 2D game\n");
                App::instance().getSceneManager()->setNext(new Game2DScene());
            })
            .background(Color(0.25f, 0.65f, 0.25f))
            .border(Color(0.15f, 0.45f, 0.15f), 4.0f)
            .foreground(Color(1.0f, 1.0f, 1.0f))
            .cornerRadius(10.0f)
            .frame(400.0f, 70.0f),
        UIView::Button("3D Game")
            .onClick([]() {
                fprintf(stderr, "[MENU] Starting 3D game\n");
                App::instance().getSceneManager()->setNext(new Game3DScene());
            })
            .background(Color(0.25f, 0.45f, 0.75f))
            .border(Color(0.15f, 0.30f, 0.55f), 4.0f)
            .foreground(Color(1.0f, 1.0f, 1.0f))
            .cornerRadius(10.0f)
            .frame(400.0f, 70.0f)
    })
    .alignment(UIAlignment::Center)
    .frame(1280.0f, 720.0f);
}

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

void MenuScene::update(double dt) {
    (void)dt;
    InputSystem* input = App::instance().getInput();
    if (!input) return;

    bool choose2D = input->isKeyPressed(SDL_SCANCODE_1) || input->isKeyPressed(SDL_SCANCODE_KP_1);
    bool choose3D = input->isKeyPressed(SDL_SCANCODE_2) || input->isKeyPressed(SDL_SCANCODE_KP_2);

    if (input->isMouseButtonPressed(1)) {
        if (ui_.handleClick(input->getMouseX(), input->getMouseY(), menu_)) {
            return;
        }
    }

    if (choose2D) {
        fprintf(stderr, "[MENU] Starting 2D game\n");
        App::instance().getSceneManager()->setNext(new Game2DScene());
    } else if (choose3D) {
        fprintf(stderr, "[MENU] Starting 3D game\n");
        App::instance().getSceneManager()->setNext(new Game3DScene());
    }
}
