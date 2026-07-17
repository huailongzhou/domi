#include "domi/app.h"
#include "domi/render.h"
#include "domi/scene_manager.h"
#include "domi/backend/sdl_backend.h"
#include "editor_scene.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <SDL3/SDL.h>
#include <cstdio>

// The Domi scene editor: same engine, plus an ImGui overlay drawn after the
// render passes (via the pre-present hook).
int main(int argc, char** argv) {
    (void)argc; (void)argv;

    domi::AppConfig config;
    config.title = "Domi Editor";
    config.width = 1600;
    config.height = 900;

    domi::App& app = domi::App::instance();
    if (!app.init(config)) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    SDL_Window* window = static_cast<SDL_Window*>(app.getBackend()->getNativeWindow());
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(app.getBackend()->getNativeRenderer());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    EditorScene* editor = new EditorScene();
    app.getSceneManager()->setNext(editor);

    app.setEventHook([](const SDL_Event& e) {
        ImGui_ImplSDL3_ProcessEvent(&e);
    });
    app.getRender()->setPrePresentHook([editor, renderer]() {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        editor->drawEditor();
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    });

    fprintf(stderr, "[EDITOR] Starting main loop\n");
    app.run();

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    app.shutdown();
    return 0;
}
