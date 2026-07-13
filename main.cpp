#include "domi/app.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "domi/script.h"
#include "domi/scene.h"
#include "domi/scene_manager.h"
#include "domi/math.h"
#include "domi/input.h"
#include "domi/canvas2d.h"
#include "domi/render_layer.h"
#include "domi/render_list.h"
#include "tree_generator.h"
#include "cloud_generator.h"
#include "rock_generator.h"
#include "house_generator.h"
#include "horizon_generator.h"
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
    Game2DScene()
        : carT_(0.0f), carSpeed_(0.25f), cloudOffset_(0.0f),
          cubeAngleX_(0.0f), cubeAngleY_(0.0f), sunEntity_(0) {}

    const char* name() const override { return "Game2DScene"; }

    void load(World* world, ScriptSystem* script) override;
    void unload(World* world, ScriptSystem* script) override;

    void update(double dt) override {
        carT_ += carSpeed_ * dt;
        if (carT_ > 1.0f) carT_ = 0.0f;

        cloudOffset_ += 8.0f * dt;
        if (cloudOffset_ > 200.0f) cloudOffset_ = 0.0f;

        cubeAngleX_ += 1.0f * dt;
        cubeAngleY_ += 0.7f * dt;

        updateCloudShadowCasters();

        if (App::instance().getInput()->isKeyPressed(SDL_SCANCODE_R)) {
            App::instance().getSceneManager()->setNext(new SecondScene());
        }
    }

    void render(Canvas2D* canvas) override {
        if (!canvas) return;

        RenderList list;

        // Horizon strip bottom edge is at y=240.
        list.add(RenderLayer::Background, 240.0f, [&, canvas]() {
            drawHorizon(canvas, 0, 120);
        });

        // Grass covers the lower 2/3 of the screen (y=240..720).
        list.add(RenderLayer::Ground, 240.0f, [&, canvas]() {
            canvas->setFillColor(Color(0.28f, 0.72f, 0.28f));
            canvas->fillRect(0, 240, 1280, 480);
        });

        // Diagonal asphalt road across the grass. Sort by its lowest y.
        list.add(RenderLayer::Road, 560.0f, [&, canvas]() {
            canvas->setFillColor(Color(0.35f, 0.35f, 0.35f));
            canvas->beginPath();
            canvas->moveTo(-121, 300);
            canvas->lineTo(-79, 360);
            canvas->lineTo(1401, 560);
            canvas->lineTo(1359, 500);
            canvas->closePath();
            canvas->fill();
        });

        // Dashed yellow center line.
        list.add(RenderLayer::RoadMarking, 560.0f, [&, canvas]() {
            canvas->setStrokeColor(Color(0.9f, 0.85f, 0.3f));
            canvas->setLineWidth(4.0f);
            for (int i = 0; i < 12; ++i) {
                float t0 = i / 12.0f;
                float t1 = (i + 0.5f) / 12.0f;
                float x0 = -100 + t0 * 1480;
                float y0 = 330 + t0 * 200;
                float x1 = -100 + t1 * 1480;
                float y1 = 330 + t1 * 200;
                canvas->beginPath();
                canvas->moveTo(x0, y0);
                canvas->lineTo(x1, y1);
                canvas->stroke();
            }
        });

        // Trees: trunks go into Object layer (sorted with car), foliage goes
        // into Canopy layer so it stays above objects/vehicles.
        addTree(list, canvas, 180, 320, 0);
        addTree(list, canvas, 1100, 300, 1);
        addTree(list, canvas, 980, 480, 2);
        addTree(list, canvas, 220, 560, 3);
        addTree(list, canvas, 1150, 620, 4);

        // House and rocks (placed on grass). Sort by bottom edge.
        list.add(RenderLayer::Object, 405.0f, [&, canvas]() { drawHouse(canvas, 560, 360); });
        list.add(RenderLayer::Object, 400.0f, [&, canvas]() { drawRock(canvas, 420, 380, 0); });
        list.add(RenderLayer::Object, 420.0f, [&, canvas]() { drawRock(canvas, 460, 400, 1); });
        list.add(RenderLayer::Object, 560.0f, [&, canvas]() { drawRock(canvas, 720, 540, 2); });
        list.add(RenderLayer::Object, 580.0f, [&, canvas]() { drawRock(canvas, 760, 560, 3); });

        // Clouds. Sort by bottom edge (center y + half height).
        list.add(RenderLayer::Cloud, 160.0f, [&, canvas]() { drawCloud(canvas, 200 + cloudOffset_, 120, 0); });
        list.add(RenderLayer::Cloud, 120.0f, [&, canvas]() { drawCloud(canvas, 600 + cloudOffset_ * 0.7f, 80, 1); });
        list.add(RenderLayer::Cloud, 190.0f, [&, canvas]() { drawCloud(canvas, 950 + cloudOffset_ * 1.2f, 150, 2); });

        // Car (follows the road center line). Put it in the Object layer so it
        // interleaves with trees/houses based on bottom-edge y-coordinate.
        float cx = -100 + carT_ * 1480;
        float cy = 330 + carT_ * 200;
        const float carHeight = 20.0f;
        list.add(RenderLayer::Object, cy + carHeight * 0.5f, [&, canvas]() { drawCar(canvas, cx, cy); });

        // A rotating 3D-style cube drawn with Canvas2D (software projection).
        list.add(RenderLayer::Effect, 550.0f, [&, canvas]() {
            drawCube3D(canvas, 1050.0f, 520.0f, 60.0f, cubeAngleX_, cubeAngleY_);
        });

        list.flush();
    }

private:
    float carT_;
    float carSpeed_;
    float cloudOffset_;
    float cubeAngleX_;
    float cubeAngleY_;

    Entity sunEntity_;
    std::vector<Entity> cloudEntities_;

    std::vector<Material> treeTrunks_;
    std::vector<Material> treeFoliages_;
    std::vector<Material> cloudMaterials_;
    std::vector<Material> rockMaterials_;
    Material houseMaterial_;
    Material horizonMaterial_;

    void createShadowCasters(World* world);
    void updateCloudShadowCasters();

    void addTree(RenderList& list, Canvas2D* canvas, float x, float y, size_t index) {
        // Sort by the tree's bottom edge (material center + half height).
        // Foliage uses the same sort key plus a tiny offset so it draws after
        // the trunk within the same tree.
        const float treeHeight = 80.0f;
        float bottomZ = y + treeHeight * 0.5f;
        list.add(RenderLayer::Object, bottomZ, [&, canvas, x, y, index]() {
            drawTreeTrunk(canvas, x, y, index);
        });
        list.add(RenderLayer::Object, bottomZ + 0.1f, [&, canvas, x, y, index]() {
            drawTreeFoliage(canvas, x, y, index);
        });
    }

    void drawTreeTrunk(Canvas2D* canvas, float x, float y, size_t index) {
        if (!canvas || index >= treeTrunks_.size()) return;
        const Material& m = treeTrunks_[index];
        if (m.width == 0) return;
        canvas->drawMaterial(x - m.width * 0.5f, y - m.height * 0.5f, m);
    }

    void drawTreeFoliage(Canvas2D* canvas, float x, float y, size_t index) {
        if (!canvas || index >= treeFoliages_.size()) return;
        const Material& m = treeFoliages_[index];
        if (m.width == 0) return;
        canvas->drawMaterial(x - m.width * 0.5f, y - m.height * 0.5f, m);
    }

    void drawCloud(Canvas2D* canvas, float x, float y, size_t index) {
        if (!canvas || index >= cloudMaterials_.size()) return;
        const Material& m = cloudMaterials_[index];
        if (m.width == 0) return;
        canvas->drawMaterial(x - m.width * 0.5f, y - m.height * 0.5f, m);
    }

    void drawRock(Canvas2D* canvas, float x, float y, size_t index) {
        if (!canvas || index >= rockMaterials_.size()) return;
        const Material& m = rockMaterials_[index];
        if (m.width == 0) return;
        canvas->drawMaterial(x - m.width * 0.5f, y - m.height * 0.5f, m);
    }

    void drawHouse(Canvas2D* canvas, float x, float y) {
        if (!canvas || houseMaterial_.width == 0) return;
        canvas->drawMaterial(x - houseMaterial_.width * 0.5f,
                             y - houseMaterial_.height * 0.5f,
                             houseMaterial_);
    }

    void drawHorizon(Canvas2D* canvas, float x, float y) {
        if (!canvas || horizonMaterial_.width == 0) return;
        canvas->drawMaterial(x, y, horizonMaterial_);
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

void Game2DScene::load(World* world, ScriptSystem* script) {
    (void)script;

    // Generate tree and cloud materials using the fluent generator API.
    // Format can be changed to PixelFormat::RGB565, PixelFormat::LUT8,
    // or PixelFormat::AlphaMask to experiment with different outputs.
    // Generate several randomized variants for each material type so that the
    // scene looks varied instead of repeating the same sprite.
    treeTrunks_.clear();
    treeFoliages_.clear();
    for (int i = 0; i < 5; ++i) {
        Material trunk, foliage;
        TreeGenerator()
            .setSize(80, 80)
            .setFormat(PixelFormat::ARGB8888)
            .setSeed(1000u + i * 137u)
            .setBaseColor(Color(0.15f, 0.55f, 0.15f, 1.0f))
            .setHighlightColor(Color(0.22f, 0.68f, 0.22f, 1.0f))
            .setDetailColor(Color(0.45f, 0.28f, 0.16f, 1.0f))
            .setTrunkSize(14, 32)
            .setFoliageRadius(30)
            .buildPair(trunk, foliage);
        treeTrunks_.push_back(trunk);
        treeFoliages_.push_back(foliage);
    }

    rockMaterials_.clear();
    for (int i = 0; i < 4; ++i) {
        rockMaterials_.push_back(RockGenerator()
            .setSize(50, 40)
            .setFormat(PixelFormat::ARGB8888)
            .setSeed(2000u + i * 93u)
            .setBaseColor(Color(0.55f, 0.55f, 0.55f, 1.0f))
            .setRockCount(4)
            .setRockRadius(16)
            .build());
    }

    houseMaterial_ = HouseGenerator()
        .setSize(90, 90)
        .setFormat(PixelFormat::ARGB8888)
        .setSeed(3000u)
        .setBaseColor(Color(0.9f, 0.85f, 0.6f, 1.0f))
        .setDetailColor(Color(0.65f, 0.25f, 0.2f, 1.0f))
        .setWallSize(55, 45)
        .setRoofHeight(28)
        .build();

    cloudMaterials_.clear();
    for (int i = 0; i < 3; ++i) {
        cloudMaterials_.push_back(CloudGenerator()
            .setSize(120, 80)
            .setFormat(PixelFormat::ARGB8888)
            .setSeed(4000u + i * 211u)
            .setBaseColor(Color(0.95f, 0.95f, 0.98f, 1.0f))
            .setPuffCount(5)
            .setPuffRadius(34)
            .build());
    }

    horizonMaterial_ = HorizonGenerator()
        .setSize(1280, 120)
        .setFormat(PixelFormat::ARGB8888)
        .setSeed(5000u)
        .setBaseColor(Color(0.28f, 0.72f, 0.28f, 1.0f))
        .setSkyColor(Color(0.12f, 0.12f, 0.16f, 1.0f))
        .setHillColor(Color(0.15f, 0.40f, 0.15f, 1.0f))
        .setHillCount(5)
        .setHillHeight(25, 70)
        .build();

    createShadowCasters(world);
}

void Game2DScene::unload(World* world, ScriptSystem* script) {
    (void)script;
    if (world) world->clear();
    sunEntity_ = 0;
    cloudEntities_.clear();
}

void Game2DScene::createShadowCasters(World* world) {
    if (!world) return;

    // Sun: directional light coming from top-right.
    sunEntity_ = world->createEntity();
    TransformComponent* sunT = world->addComponent<TransformComponent>(sunEntity_);
    sunT->transform.position = Vec3(1.0f, -1.0f, 0.0f);
    LightComponent* sunL = world->addComponent<LightComponent>(sunEntity_);
    sunL->type = LightComponent::Directional;
    sunL->color = Color(1.0f, 0.95f, 0.85f);
    sunL->intensity = 1.0f;

    // Clouds: alpha-zero sprites that only cast shadows.
    cloudEntities_.clear();
    for (int i = 0; i < 3; ++i) {
        Entity cloud = world->createEntity();
        TransformComponent* t = world->addComponent<TransformComponent>(cloud);
        t->transform.scale = Vec3(1.2f, 0.8f, 1.0f);
        SpriteComponent* s = world->addComponent<SpriteComponent>(cloud);
        s->color = Color(0.0f, 0.0f, 0.0f, 0.0f); // invisible, shadow caster only
        s->texturePath = "cloud_shadow_caster";   // required by ECS query mask
        cloudEntities_.push_back(cloud);
    }
    updateCloudShadowCasters();
}

void Game2DScene::updateCloudShadowCasters() {
    World* world = App::instance().getWorld();
    if (!world || cloudEntities_.size() < 3) return;

    struct CloudPos { float x, y; };
    CloudPos positions[3] = {
        { 200.0f + cloudOffset_, 120.0f },
        { 600.0f + cloudOffset_ * 0.7f, 80.0f },
        { 950.0f + cloudOffset_ * 1.2f, 150.0f }
    };

    for (int i = 0; i < 3; ++i) {
        TransformComponent* t = world->getComponent<TransformComponent>(cloudEntities_[i]);
        if (t) {
            t->transform.position.x = positions[i].x;
            t->transform.position.y = positions[i].y;
        }
    }
}

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
