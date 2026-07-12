#include "domi/app.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "domi/script.h"
#include "domi/scene.h"
#include "domi/scene_manager.h"
#include "domi/math.h"
#include "domi/input.h"
#include "domi/canvas2d.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

using namespace domi;

class Game2DScene;
class Game3DScene;
class MenuScene;

class SecondScene : public Scene {
public:
    SecondScene() : blockEntity_(0) {}
    const char* name() const override { return "SecondScene"; }

    void load(World* world, ScriptSystem* script) override;
    void unload(World* world, ScriptSystem* script) override;
    void update(double dt) override;

private:
    Entity blockEntity_;
};

class Game2DScene : public Scene {
public:
    Game2DScene() : carT_(0.0f), carSpeed_(0.25f), cloudOffset_(0.0f), cubeAngleX_(0.0f), cubeAngleY_(0.0f) {}

    const char* name() const override { return "Game2DScene"; }

    void load(World* world, ScriptSystem* script) override {
        (void)world;
        (void)script;
    }

    void unload(World* world, ScriptSystem* script) override {
        (void)world;
        (void)script;
    }

    void update(double dt) override {
        carT_ += carSpeed_ * dt;
        if (carT_ > 1.0f) carT_ = 0.0f;

        cloudOffset_ += 8.0f * dt;
        if (cloudOffset_ > 200.0f) cloudOffset_ = 0.0f;

        cubeAngleX_ += 1.0f * dt;
        cubeAngleY_ += 0.7f * dt;

        if (App::instance().getInput()->isKeyPressed(SDL_SCANCODE_R)) {
            App::instance().getSceneManager()->setNext(new SecondScene());
        }
    }

    void render(Canvas2D* canvas) override {
        if (!canvas) return;

        // Grass background
        canvas->setFillColor(Color(0.28f, 0.72f, 0.28f));
        canvas->fillRect(0, 0, 1280, 720);

        // Trees
        drawTree(canvas, 180, 200);
        drawTree(canvas, 1100, 160);
        drawTree(canvas, 980, 450);
        drawTree(canvas, 220, 520);
        drawTree(canvas, 1150, 600);

        // Diagonal asphalt road crossing the screen
        canvas->setFillColor(Color(0.35f, 0.35f, 0.35f));
        canvas->beginPath();
        canvas->moveTo(-121, 786);
        canvas->lineTo(-79, 854);
        canvas->lineTo(1401, -66);
        canvas->lineTo(1359, -134);
        canvas->closePath();
        canvas->fill();

        // Dashed yellow center line
        canvas->setStrokeColor(Color(0.9f, 0.85f, 0.3f));
        canvas->setLineWidth(4.0f);
        for (int i = 0; i < 12; ++i) {
            float t0 = i / 12.0f;
            float t1 = (i + 0.5f) / 12.0f;
            float x0 = -100 + t0 * 1480;
            float y0 = 820 - t0 * 920;
            float x1 = -100 + t1 * 1480;
            float y1 = 820 - t1 * 920;
            canvas->beginPath();
            canvas->moveTo(x0, y0);
            canvas->lineTo(x1, y1);
            canvas->stroke();
        }

        // Clouds
        drawCloud(canvas, 200 + cloudOffset_, 120);
        drawCloud(canvas, 600 + cloudOffset_ * 0.7f, 80);
        drawCloud(canvas, 950 + cloudOffset_ * 1.2f, 150);

        // Car
        float cx = -100 + carT_ * 1480;
        float cy = 820 - carT_ * 920;
        drawCar(canvas, cx, cy);

        // A rotating 3D-style cube drawn with Canvas2D (software projection).
        drawCube3D(canvas, 1050.0f, 200.0f, 60.0f, cubeAngleX_, cubeAngleY_);
    }

private:
    float carT_;
    float carSpeed_;
    float cloudOffset_;
    float cubeAngleX_;
    float cubeAngleY_;

    void drawTree(Canvas2D* canvas, float x, float y) {
        canvas->setFillColor(Color(0.45f, 0.28f, 0.16f));
        canvas->fillRect(x - 6, y + 20, 12, 30);

        canvas->setFillColor(Color(0.15f, 0.55f, 0.15f));
        canvas->fillCircle(x, y, 28);
        canvas->setFillColor(Color(0.22f, 0.68f, 0.22f));
        canvas->fillCircle(x - 10, y + 8, 18);
        canvas->fillCircle(x + 10, y + 8, 18);
    }

    void drawCloud(Canvas2D* canvas, float x, float y) {
        canvas->setFillColor(Color(0.95f, 0.95f, 0.98f));
        canvas->fillCircle(x, y, 32);
        canvas->fillCircle(x + 28, y + 8, 26);
        canvas->fillCircle(x + 52, y - 4, 22);
        canvas->fillCircle(x + 22, y - 14, 24);
    }

    void drawCar(Canvas2D* canvas, float x, float y) {
        canvas->setFillColor(Color(0.85f, 0.2f, 0.2f));
        canvas->fillRect(x - 18, y - 10, 36, 20);

        canvas->setFillColor(Color(0.2f, 0.25f, 0.4f));
        canvas->fillRect(x - 10, y - 16, 20, 10);

        canvas->setFillColor(Color(0.1f, 0.1f, 0.1f));
        canvas->fillCircle(x - 12, y + 10, 5);
        canvas->fillCircle(x + 12, y + 10, 5);
    }

    void drawCube3D(Canvas2D* canvas, float cx, float cy, float size, float ax, float ay) {
        // Local cube vertices: centered at origin, edge length 1.
        Vec3 v[8] = {
            Vec3(-0.5f, -0.5f, -0.5f), Vec3( 0.5f, -0.5f, -0.5f),
            Vec3( 0.5f,  0.5f, -0.5f), Vec3(-0.5f,  0.5f, -0.5f),
            Vec3(-0.5f, -0.5f,  0.5f), Vec3( 0.5f, -0.5f,  0.5f),
            Vec3( 0.5f,  0.5f,  0.5f), Vec3(-0.5f,  0.5f,  0.5f)
        };

        // Faces defined counter-clockwise when viewed from outside.
        // Winding order verified so that cross(edge1, edge2) points outward.
        struct FaceDef { int i[4]; Color color; };
        FaceDef faces[6] = {
            { {0, 3, 2, 1}, Color(1.0f, 0.2f, 0.2f) }, // front  (-Z)
            { {5, 6, 7, 4}, Color(0.2f, 1.0f, 0.2f) }, // back   (+Z)
            { {3, 7, 6, 2}, Color(0.2f, 0.2f, 1.0f) }, // top    (+Y)
            { {4, 0, 1, 5}, Color(1.0f, 1.0f, 0.2f) }, // bottom (-Y)
            { {1, 2, 6, 5}, Color(1.0f, 0.2f, 1.0f) }, // right  (+X)
            { {4, 7, 3, 0}, Color(0.2f, 1.0f, 1.0f) }  // left   (-X)
        };

        // Precompute rotation.
        float cxr = SDL_cosf(ax), sxr = SDL_sinf(ax);
        float cyr = SDL_cosf(ay), syr = SDL_sinf(ay);

        auto rotate = [&](const Vec3& p) -> Vec3 {
            Vec3 r1(p.x, p.y * cxr - p.z * sxr, p.y * sxr + p.z * cxr);
            return Vec3(r1.x * cyr + r1.z * syr, r1.y, -r1.x * syr + r1.z * cyr);
        };

        const float cameraDist = 3.0f;
        const float projScale = size;

        auto project = [&](const Vec3& p) -> Vec2 {
            float denom = p.z + cameraDist;
            if (denom < 0.01f) denom = 0.01f;
            return Vec2(cx + p.x / denom * projScale, cy - p.y / denom * projScale);
        };

        // Transform vertices once.
        Vec3 tv[8];
        for (int i = 0; i < 8; ++i) tv[i] = rotate(v[i]);

        struct VisibleFace {
            int i[4];
            Color color;
            float depth;
            float light;
        };
        VisibleFace visible[6];
        int visibleCount = 0;

        for (int f = 0; f < 6; ++f) {
            const FaceDef& face = faces[f];
            Vec3 a = tv[face.i[0]];
            Vec3 b = tv[face.i[1]];
            Vec3 c_ = tv[face.i[2]];

            // Camera looks down +Z from -cameraDist; visible faces have outward normal.z < 0.
            Vec3 n = Vec3::cross(b - a, c_ - a);
            if (n.z >= 0.0f) continue;

            float l = -n.z / n.length();
            if (l < 0.3f) l = 0.3f;
            if (l > 1.0f) l = 1.0f;

            float depth = (a.z + b.z + c_.z + tv[face.i[3]].z) * 0.25f;
            VisibleFace& vf = visible[visibleCount++];
            for (int k = 0; k < 4; ++k) vf.i[k] = face.i[k];
            vf.color = face.color;
            vf.depth = depth;
            vf.light = l;
        }

        // Painter's algorithm: draw farther faces first (larger depth first).
        for (int i = 0; i < visibleCount - 1; ++i) {
            for (int j = i + 1; j < visibleCount; ++j) {
                if (visible[i].depth < visible[j].depth) {
                    VisibleFace tmp = visible[i];
                    visible[i] = visible[j];
                    visible[j] = tmp;
                }
            }
        }

        for (int f = 0; f < visibleCount; ++f) {
            const VisibleFace& vf = visible[f];
            canvas->setFillColor(Color(vf.color.r * vf.light, vf.color.g * vf.light, vf.color.b * vf.light));
            canvas->beginPath();
            Vec2 p0 = project(tv[vf.i[0]]);
            canvas->moveTo(p0.x, p0.y);
            for (int k = 1; k < 4; ++k) {
                Vec2 p = project(tv[vf.i[k]]);
                canvas->lineTo(p.x, p.y);
            }
            canvas->closePath();
            canvas->fill();
        }
    }
};

class Game3DScene : public Scene {
public:
    Game3DScene() : player_(0), score_(0) {
        std::srand(static_cast<unsigned>(std::time(NULL)));
    }

    const char* name() const override { return "Game3DScene"; }

    void load(World* world, ScriptSystem* script) override {
        (void)script;

        // Ground cube platform
        Entity ground = world->createEntity();
        TransformComponent* groundT = world->addComponent<TransformComponent>(ground);
        groundT->transform.position = Vec3(0.0f, -0.4f, 0.0f);
        groundT->transform.scale = Vec3(3.0f, 0.8f, 3.0f);
        MeshComponent* groundMesh = world->addComponent<MeshComponent>(ground);
        groundMesh->modelPath = "builtin:cube";
        groundMesh->color = Color(0.35f, 0.35f, 0.35f);

        // Player cube
        player_ = world->createEntity();
        TransformComponent* playerT = world->addComponent<TransformComponent>(player_);
        playerT->transform.position = Vec3(0.0f, 0.35f, 0.0f);
        playerT->transform.scale = Vec3(0.35f, 0.35f, 0.35f);
        MeshComponent* playerMesh = world->addComponent<MeshComponent>(player_);
        playerMesh->modelPath = "builtin:cube";
        playerMesh->color = Color(0.2f, 0.6f, 1.0f);

        // Camera
        Entity camera = world->createEntity();
        TransformComponent* camT = world->addComponent<TransformComponent>(camera);
        camT->transform.position = Vec3(5.0f, 5.0f, 5.0f);
        CameraComponent* cam = world->addComponent<CameraComponent>(camera);
        cam->isPerspective = false;
        cam->size = 3.5f;
        cam->isActive = true;

        spawnCollectibles(world);
        score_ = 0;
    }

    void unload(World* world, ScriptSystem* script) override {
        (void)script;
        if (world) world->clear();
        collectibles_.clear();
        score_ = 0;
    }

    void update(double dt) override {
        World* world = App::instance().getWorld();
        InputSystem* input = App::instance().getInput();
        if (!world || !input) return;

        const float moveSpeed = 4.0f;
        const float limit = 2.5f;
        float h = input->getAxis("Horizontal");
        float v = input->getAxis("Vertical");

        TransformComponent* pt = world->getComponent<TransformComponent>(player_);
        if (pt) {
            pt->transform.position.x += h * moveSpeed * dt;
            pt->transform.position.z += v * moveSpeed * dt;
            if (pt->transform.position.x < -limit) pt->transform.position.x = -limit;
            if (pt->transform.position.x >  limit) pt->transform.position.x =  limit;
            if (pt->transform.position.z < -limit) pt->transform.position.z = -limit;
            if (pt->transform.position.z >  limit) pt->transform.position.z =  limit;
        }

        float time = App::instance().getTime();
        for (size_t i = 0; i < collectibles_.size(); ++i) {
            Entity e = collectibles_[i];
            if (!world->isValid(e)) continue;
            TransformComponent* t = world->getComponent<TransformComponent>(e);
            if (t) {
                float angle = -(time * 2.0f + static_cast<float>(i));
                t->transform.rotation = Quat::fromEuler(0.0f, angle, 0.0f);
            }
        }

        if (pt) {
            Vec3 ppos = pt->transform.position;
            for (size_t i = 0; i < collectibles_.size();) {
                Entity e = collectibles_[i];
                if (!world->isValid(e)) {
                    collectibles_.erase(collectibles_.begin() + i);
                    continue;
                }
                TransformComponent* t = world->getComponent<TransformComponent>(e);
                if (!t) {
                    collectibles_.erase(collectibles_.begin() + i);
                    continue;
                }
                float dx = ppos.x - t->transform.position.x;
                float dz = ppos.z - t->transform.position.z;
                if (dx * dx + dz * dz < 0.3f * 0.3f) {
                    world->destroyEntity(e);
                    collectibles_.erase(collectibles_.begin() + i);
                    ++score_;
                    fprintf(stderr, "[GAME] Score: %d\n", score_);
                } else {
                    ++i;
                }
            }
        }

        if (collectibles_.empty()) {
            spawnCollectibles(world);
            fprintf(stderr, "[GAME] New wave! Score: %d\n", score_);
        }

        if (input->isKeyPressed(SDL_SCANCODE_R)) {
            App::instance().getSceneManager()->setNext(new Game2DScene());
        }
    }

private:
    Entity player_;
    std::vector<Entity> collectibles_;
    int score_;

    void spawnCollectibles(World* world) {
        const int count = 5;
        for (int i = 0; i < count; ++i) {
            Entity c = world->createEntity();
            TransformComponent* t = world->addComponent<TransformComponent>(c);
            float radius = 0.8f + (std::rand() % 1200) / 1000.0f;
            float angle = (std::rand() % 6283) / 1000.0f;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            t->transform.position = Vec3(x, 0.3f, z);
            t->transform.scale = Vec3(0.25f, 0.25f, 0.25f);
            MeshComponent* collectibleMesh = world->addComponent<MeshComponent>(c);
            collectibleMesh->modelPath = "builtin:cube";
            collectibleMesh->color = Color(1.0f, 0.9f, 0.15f);
            collectibles_.push_back(c);
        }
    }
};

class MenuScene : public Scene {
public:
    MenuScene() : button2DY_(320), button3DY_(430), buttonW_(400), buttonH_(70) {}

    const char* name() const override { return "MenuScene"; }

    void load(World* world, ScriptSystem* script) override {
        (void)world;
        (void)script;
    }

    void unload(World* world, ScriptSystem* script) override {
        (void)world;
        (void)script;
    }

    void update(double dt) override {
        (void)dt;
        InputSystem* input = App::instance().getInput();
        if (!input) return;

        bool choose2D = input->isKeyPressed(SDL_SCANCODE_1) || input->isKeyPressed(SDL_SCANCODE_KP_1);
        bool choose3D = input->isKeyPressed(SDL_SCANCODE_2) || input->isKeyPressed(SDL_SCANCODE_KP_2);

        // Also support mouse clicks on the buttons.
        if (input->isMouseButtonPressed(1)) {
            float mx = input->getMouseX();
            float my = input->getMouseY();
            if (hitButton(mx, my, button2DY_)) {
                choose2D = true;
                fprintf(stderr, "[MENU] Clicked 2D button\n");
            } else if (hitButton(mx, my, button3DY_)) {
                choose3D = true;
                fprintf(stderr, "[MENU] Clicked 3D button\n");
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

    void render(Canvas2D* canvas) override {
        if (!canvas) return;

        // Background
        canvas->setFillColor(Color(0.08f, 0.08f, 0.12f));
        canvas->fillRect(0, 0, 1280, 720);

        // Title bar
        canvas->setFillColor(Color(0.9f, 0.9f, 0.9f));
        canvas->fillRect(340, 160, 600, 80);

        drawButton(canvas, button2DY_, Color(0.25f, 0.65f, 0.25f), Color(0.15f, 0.45f, 0.15f));
        drawButton(canvas, button3DY_, Color(0.25f, 0.45f, 0.75f), Color(0.15f, 0.30f, 0.55f));

        // Hint bars representing "Press 1 or click for 2D, Press 2 or click for 3D"
        canvas->setFillColor(Color(0.7f, 0.7f, 0.7f));
        canvas->fillRect(440, 560, 400, 6);
        canvas->fillRect(440, 580, 400, 6);
        canvas->fillRect(440, 600, 280, 6);
    }

private:
    float button2DY_, button3DY_;
    float buttonW_, buttonH_;

    bool hitButton(float mx, float my, float by) const {
        float bx = (1280.0f - buttonW_) * 0.5f;
        return mx >= bx && mx <= bx + buttonW_ && my >= by && my <= by + buttonH_;
    }

    void drawButton(Canvas2D* canvas, float by, const Color& base, const Color& border) const {
        float bx = (1280.0f - buttonW_) * 0.5f;
        canvas->setFillColor(border);
        canvas->fillRect(bx - 4, by - 4, buttonW_ + 8, buttonH_ + 8);
        canvas->setFillColor(base);
        canvas->fillRect(bx, by, buttonW_, buttonH_);
        canvas->setFillColor(Color(1.0f, 1.0f, 1.0f));
        canvas->fillRect(bx + 10, by + 10, buttonW_ - 20, buttonH_ - 20);
    }
};

void SecondScene::load(World* world, ScriptSystem* script) {
    blockEntity_ = world->createEntity();
    world->addComponent<TransformComponent>(blockEntity_);
    world->addComponent<SpriteComponent>(blockEntity_)->color = Color(1.0f, 0.2f, 0.2f, 1.0f);

    if (script) {
        script->loadScript(blockEntity_, "scripts/player_2d.wasm");
    }
}

void SecondScene::unload(World* world, ScriptSystem* script) {
    if (script && blockEntity_ != 0) {
        script->unloadScript(blockEntity_);
        blockEntity_ = 0;
    }
    if (world) world->clear();
}

void SecondScene::update(double dt) {
    (void)dt;
    if (App::instance().getInput()->isKeyPressed(SDL_SCANCODE_R)) {
        App::instance().getSceneManager()->setNext(new Game2DScene());
    }
}

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
