#include "domi/app.h"
#include "domi/scene_manager.h"
#include "domi/script.h"
#include "demo/menu_scene.h"
#include <cstdio>

using namespace domi;

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    AppConfig config;
    config.title = "Domi Engine";
    config.width = 1280;
    config.height = 720;

    App& app = App::instance();
    if (!app.init(config)) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    app.getSceneManager()->setNext(new MenuScene());
    app.getScript()->setThreadPool(app.getThreadPool());

    fprintf(stderr, "[MAIN] Starting main loop\n");
    app.run();
    app.shutdown();
    return 0;
}
