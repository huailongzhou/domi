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
#include "domi/scene_function.h"
#include "domi/ui/ui.h"
#include "tree_generator.h"
#include "cloud_generator.h"
#include "rock_generator.h"
#include "house_generator.h"
#include "horizon_generator.h"
#include "3d/cube3d_generator.h"
#include "3d/car3d_generator.h"
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
    static constexpr int kCloudCount = 6;
    static constexpr float kHorizonY = 240.0f;

    Game2DScene()
        : carT_(0.0f), carSpeed_(0.25f), cloudOffset_(0.0f),
          cubeAngleX_(0.0f), cubeAngleY_(0.0f), sunEntity_(0),
          useZBuffer_(true), dayTime_(6.0f), dayDuration_(120.0f),
          cachedSunColor_(1.0f, 0.95f, 0.85f, 1.0f),
          cachedSunIntensity_(1.0f),
          cachedLightDir_(0.0f, -1.0f),
          cachedOverlay_(1.0f, 1.0f, 1.0f, 0.0f) {}

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

        // Advance the day/night cycle: 24 hours in dayDuration_ seconds.
        dayTime_ += static_cast<float>(dt * (24.0 / dayDuration_));
        if (dayTime_ >= 24.0f) dayTime_ -= 24.0f;

        // Evaluate the day/night cycle once per frame; every other system
        // reads the cached result instead of recomputing it.
        evaluateDayCycle(cachedSunColor_, cachedSunIntensity_,
                         cachedLightDir_, cachedOverlay_);
        updateSun();
        updateCloudShadowCasters();

        if (App::instance().getInput()->isKeyPressed(SDL_SCANCODE_R)) {
            App::instance().getSceneManager()->setNext(new SecondScene());
        }

        if (App::instance().getInput()->isKeyPressed(SDL_SCANCODE_Z)) {
            useZBuffer_ = !useZBuffer_;
            fprintf(stderr, "[GAME2D] Render mode: %s\n",
                    useZBuffer_ ? "z-buffer" : "painter");
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

        // Ground-object shadows are drawn after the road surface but before
        // objects, so they are visible on both grass and asphalt. Cloud shadows
        // stay in the ShadowPass shadow mask.
        list.add(RenderLayer::Surface, 562.0f, [&, canvas]() {
            Vec2 shadowDir = cachedLightDir_ * -1.0f;
            if (shadowDir.y > 0.0f) {
                // Tree shadows scale with the tree. The shadow base is fixed at
                // the tree's bottom (y + 40) while the shadow size shrinks with distance.
                struct TreePos { float x, y; };
                TreePos trees[] = {
                    { 180, 320 }, { 1100, 300 }, { 980, 480 },
                    { 220, 560 }, { 1150, 620 }
                };
                for (const auto& t : trees) {
                    float s = perspectiveScale(t.y);
                    SceneFunction::drawShadow(canvas, t.x, t.y + 40.0f,
                                              32.0f * s, 10.0f * s, 0.0f,
                                              shadowDir);
                }
                // House shadow base is at y + 45 (90px tall, centered).
                SceneFunction::drawShadow(canvas, 560,  360, 48.0f, 16.0f, 45.0f, shadowDir);
                // Cube shadow base is at y + 30 (60px size, centered).
                SceneFunction::drawShadow(canvas, 1050, 520, 34.0f, 12.0f, 30.0f, shadowDir);
            }
        });

        // Diagonal asphalt road across the grass. Sort by its lowest y.
        list.add(RenderLayer::Surface, 560.0f, [&, canvas]() {
            canvas->setFillColor(Color(0.35f, 0.35f, 0.35f));
            canvas->beginPath();
            canvas->moveTo(-121, 300);
            canvas->lineTo(-79, 360);
            canvas->lineTo(1401, 560);
            canvas->lineTo(1359, 500);
            canvas->closePath();
            canvas->fill();
        });

        // Dashed yellow center line. Drawn just above the asphalt so it is not
        // accidentally sorted underneath it when the two items share a layer.
        list.add(RenderLayer::Surface, 561.0f, [&, canvas]() {
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

        // Clouds. Sort by center y so lower (farther back) clouds draw first.
        // All y values are within the sky band; some overlap the hills while
        // others sit clearly above them.
        list.add(RenderLayer::Background, 100.0f, [&, canvas]() { drawCloud(canvas, 150 + cloudOffset_, 100, 0); });
        list.add(RenderLayer::Background,  80.0f, [&, canvas]() { drawCloud(canvas, 450 + cloudOffset_ * 0.7f, 80, 1); });
        list.add(RenderLayer::Background, 130.0f, [&, canvas]() { drawCloud(canvas, 800 + cloudOffset_ * 1.2f, 130, 2); });
        list.add(RenderLayer::Background, 170.0f, [&, canvas]() { drawCloud(canvas, 250 + cloudOffset_ * 0.5f, 170, 3); });
        list.add(RenderLayer::Background, 200.0f, [&, canvas]() { drawCloud(canvas, 650 + cloudOffset_ * 0.9f, 200, 4); });
        list.add(RenderLayer::Background, 230.0f, [&, canvas]() { drawCloud(canvas, 1050 + cloudOffset_ * 1.1f, 230, 5); });

        // 3D car: follows the road, drawn in the Object layer.
        float cx = -100 + carT_ * 1480;
        float cy = 330 + carT_ * 200;
        list.add(RenderLayer::Object, cy, [&, canvas, cx, cy]() {
            canvas->setRenderMode(useZBuffer_ ? RenderMode::ZBUFFER : RenderMode::PAINTER);
            canvas->begin3D();
            draw3DCar(canvas, cx, cy);
            canvas->end3D();
        });

        // 3D rotating cube: drawn as a screen-space effect on top of the scene.
        list.add(RenderLayer::Effect, 550.0f, [&, canvas]() {
            canvas->setRenderMode(useZBuffer_ ? RenderMode::ZBUFFER : RenderMode::PAINTER);
            canvas->begin3D();
            drawCube3D(canvas, 1050.0f, 520.0f, 60.0f, cubeAngleX_, cubeAngleY_);
            canvas->end3D();
        });

        // Day/night color overlay.
        list.add(RenderLayer::Overlay, 0.0f, [&, canvas]() {
            canvas->setFillColor(cachedOverlay_);
            canvas->fillRect(0, 0, 1280, 720);
        });

        list.flush();
    }

private:
    float carT_;
    float carSpeed_;
    float cloudOffset_;
    float cubeAngleX_;
    float cubeAngleY_;
    bool useZBuffer_;
    float dayTime_;      // 0..24 hours
    float dayDuration_;  // seconds for a full 24-hour cycle

    // Cached day-cycle state, computed once per frame in update().
    Color cachedSunColor_;
    float cachedSunIntensity_;
    Vec2 cachedLightDir_;
    Color cachedOverlay_;

    Entity sunEntity_;
    std::vector<Entity> cloudEntities_;

    std::vector<Material> treeTrunks_;
    std::vector<Material> treeFoliages_;

    static Color lerpColor(const Color& a, const Color& b, float t);
    void evaluateDayCycle(Color& outSunColor, float& outIntensity,
                          Vec2& outLightDir, Color& outOverlay) const;
    void updateSun();
    std::vector<Material> cloudMaterials_;
    std::vector<Material> rockMaterials_;
    Material houseMaterial_;
    Material horizonMaterial_;
    std::vector<int> horizonSkyline_; // texture-local hill-top y per x column

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

    float perspectiveScale(float y) const {
        // Objects near the horizon (y=240) are far away and smaller;
        // objects near the bottom (y=720) are close and full size.
        const float minScale = 0.35f;
        const float maxScale = 1.0f;
        float t = (y - kHorizonY) / (720.0f - kHorizonY);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return minScale + t * (maxScale - minScale);
    }

    void drawTreeTrunk(Canvas2D* canvas, float x, float y, size_t index) {
        if (!canvas || index >= treeTrunks_.size()) return;
        const Material& m = treeTrunks_[index];
        if (m.width == 0) return;
        float s = perspectiveScale(y);
        float bottomY = y + 40.0f; // tree center y + half original height
        canvas->save();
        canvas->translate(x, bottomY);
        canvas->scale(s, s);
        canvas->drawMaterial(-m.width * 0.5f, -m.height, m);
        canvas->restore();
    }

    void drawTreeFoliage(Canvas2D* canvas, float x, float y, size_t index) {
        if (!canvas || index >= treeFoliages_.size()) return;
        const Material& m = treeFoliages_[index];
        if (m.width == 0) return;
        float s = perspectiveScale(y);
        float bottomY = y + 40.0f; // tree center y + half original height
        canvas->save();
        canvas->translate(x, bottomY);
        canvas->scale(s, s);
        canvas->drawMaterial(-m.width * 0.5f, -m.height, m);
        canvas->restore();
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

    void draw3DCar(Canvas2D* canvas, float x, float y) {
        if (!canvas) return;
        float time = (float)App::instance().getTime();
        float yaw = carT_ * 0.25f;
        float roll = SDL_sinf(time * 10.0f) * 0.03f;

        static const std::vector<Mesh3D> carMeshes = Car3DGenerator().build();

        for (const Mesh3D& mesh : carMeshes) {
            canvas->drawMesh3D(x, y, 1.0f, 0.0f, 0.0f, yaw + roll,
                               mesh.vertices.data(), (int)mesh.vertices.size(),
                               mesh.indices.data(), mesh.triangleCount(),
                               mesh.color);
        }
    }

    void drawCube3D(Canvas2D* canvas, float cx, float cy, float size, float ax, float ay) {
        if (!canvas) return;
        static const std::vector<Mesh3D> cubeFaces = Cube3DGenerator().setSize(1.0f).build();

        for (const Mesh3D& face : cubeFaces) {
            canvas->drawMesh3D(cx, cy, size, ax, ay, 0.0f,
                               face.vertices.data(), (int)face.vertices.size(),
                               face.indices.data(), face.triangleCount(),
                               face.color);
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
    for (int i = 0; i < kCloudCount; ++i) {
        cloudMaterials_.push_back(CloudGenerator()
            .setSize(120, 80)
            .setFormat(PixelFormat::ARGB8888)
            .setSeed(4000u + i * 211u)
            .setBaseColor(Color(0.95f, 0.95f, 0.98f, 1.0f))
            .setPuffCount(5)
            .setPuffRadius(34)
            .build());
    }

    HorizonGenerator horizonGen;
    horizonGen.setSize(1280, 120)
        .setFormat(PixelFormat::ARGB8888)
        .setSeed(5000u)
        .setBaseColor(Color(0.28f, 0.72f, 0.28f, 1.0f))
        .setSkyColor(Color(0.12f, 0.12f, 0.16f, 1.0f))
        .setHillColor(Color(0.15f, 0.40f, 0.15f, 1.0f))
        .setHillCount(5)
        .setHillHeight(25, 70);
    horizonMaterial_ = horizonGen.build();
    horizonSkyline_ = horizonGen.buildSkyline();

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

    // Sun: directional light. Direction is stored in the transform's forward
    // vector and updated each frame by updateSun(); position is irrelevant for
    // a directional light, so keep it at the origin.
    sunEntity_ = world->createEntity();
    TransformComponent* sunT = world->addComponent<TransformComponent>(sunEntity_);
    sunT->transform.position = Vec3(0.0f, 0.0f, 0.0f);
    LightComponent* sunL = world->addComponent<LightComponent>(sunEntity_);
    sunL->type = LightComponent::Directional;
    sunL->color = Color(1.0f, 0.95f, 0.85f);
    sunL->intensity = 1.0f;

    // Clouds: alpha-zero sprites that only cast shadows.
    cloudEntities_.clear();
    for (int i = 0; i < kCloudCount; ++i) {
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
    if (!world || cloudEntities_.size() < (size_t)kCloudCount) return;

    struct CloudPos { float x, y; };
    CloudPos positions[kCloudCount] = {
        { 150.0f + cloudOffset_,          100.0f },
        { 450.0f + cloudOffset_ * 0.7f,    80.0f },
        { 800.0f + cloudOffset_ * 1.2f,   130.0f },
        { 250.0f + cloudOffset_ * 0.5f,   170.0f },
        { 650.0f + cloudOffset_ * 0.9f,   200.0f },
        { 1050.0f + cloudOffset_ * 1.1f,  230.0f }
    };

    const float horizonDrawY = 120.0f;
    const float cloudHalfH = 40.0f;
    const int skylineW = static_cast<int>(horizonSkyline_.size());

    // No ground shadows when the sun is below the horizon.
    bool sunAboveHorizon = (cachedLightDir_.y < 0.0f);

    for (int i = 0; i < kCloudCount; ++i) {
        TransformComponent* t = world->getComponent<TransformComponent>(cloudEntities_[i]);
        SpriteComponent* s = world->getComponent<SpriteComponent>(cloudEntities_[i]);
        if (t) {
            t->transform.position.x = positions[i].x;
            t->transform.position.y = positions[i].y;
        }

        // A cloud casts a shadow only while the sun is up and the cloud is
        // clearly above the horizon hills.
        if (s && skylineW > 0) {
            int idx = static_cast<int>(positions[i].x);
            if (idx < 0) idx = 0;
            if (idx >= skylineW) idx = skylineW - 1;
            float cloudBottom = positions[i].y + cloudHalfH;
            float screenSkyline = horizonDrawY + static_cast<float>(horizonSkyline_[idx]);
            s->castShadow = sunAboveHorizon && (cloudBottom < screenSkyline);
        }
    }
}

Color Game2DScene::lerpColor(const Color& a, const Color& b, float t) {
    return Color(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t);
}

// Evaluate the day/night cycle at dayTime_ (0..24).
// Returns sun light color/intensity, sun direction, and a screen overlay tint.
void Game2DScene::evaluateDayCycle(Color& outSunColor, float& outIntensity,
                                   Vec2& outLightDir, Color& outOverlay) const {
        const float t = dayTime_ / 24.0f;

        // Cycle phases: night -> dawn -> day -> dusk -> night.
        Color nightOverlay(0.05f, 0.05f, 0.20f, 0.65f);
        Color dawnOverlay  (0.90f, 0.50f, 0.35f, 0.25f);
        Color dayOverlay   (1.00f, 1.00f, 1.00f, 0.00f);
        Color duskOverlay  (0.85f, 0.35f, 0.20f, 0.30f);

        Color nightSun(0.20f, 0.20f, 0.40f, 1.0f);
        Color dawnSun  (1.00f, 0.55f, 0.25f, 1.0f);
        Color daySun   (1.00f, 0.95f, 0.80f, 1.0f);
        Color duskSun  (1.00f, 0.45f, 0.20f, 1.0f);

        if (t < 0.20f) {
            // 00:00 - 05:00: night -> dawn prep
            float local = t / 0.20f;
            outOverlay = lerpColor(nightOverlay, nightOverlay, local);
            outSunColor = nightSun;
            outIntensity = 0.15f;
        } else if (t < 0.30f) {
            // 05:00 - 07:00: dawn
            float local = (t - 0.20f) / 0.10f;
            outOverlay = lerpColor(nightOverlay, dawnOverlay, local);
            outSunColor = lerpColor(nightSun, dawnSun, local);
            outIntensity = 0.15f + 0.45f * local;
        } else if (t < 0.45f) {
            // 07:00 - 11:00: dawn -> day
            float local = (t - 0.30f) / 0.15f;
            outOverlay = lerpColor(dawnOverlay, dayOverlay, local);
            outSunColor = lerpColor(dawnSun, daySun, local);
            outIntensity = 0.60f + 0.40f * local;
        } else if (t < 0.55f) {
            // 11:00 - 13:00: midday
            outOverlay = dayOverlay;
            outSunColor = daySun;
            outIntensity = 1.00f;
        } else if (t < 0.70f) {
            // 13:00 - 17:00: day -> dusk
            float local = (t - 0.55f) / 0.15f;
            outOverlay = lerpColor(dayOverlay, duskOverlay, local);
            outSunColor = lerpColor(daySun, duskSun, local);
            outIntensity = 1.00f - 0.25f * local;
        } else if (t < 0.80f) {
            // 17:00 - 19:00: dusk
            float local = (t - 0.70f) / 0.10f;
            outOverlay = lerpColor(duskOverlay, nightOverlay, local);
            outSunColor = lerpColor(duskSun, nightSun, local);
            outIntensity = 0.75f - 0.60f * local;
        } else {
            // 19:00 - 24:00: night
            float local = (t - 0.80f) / 0.20f;
            outOverlay = lerpColor(nightOverlay, nightOverlay, local);
            outSunColor = nightSun;
            outIntensity = 0.15f;
        }

        // Sun direction: sunrise at 6:00, noon at 12:00, sunset at 18:00.
        // Angle 0 = sun on right/east, pi/2 = top, pi = left/west.
        float sunAngle = (dayTime_ - 6.0f) / 12.0f * 3.14159265f;
        if (sunAngle < 0.0f || sunAngle > 3.14159265f) {
            // Night: light comes from below, no ground shadows.
            outLightDir = Vec2(0.0f, 1.0f);
        } else {
            outLightDir = Vec2(-SDL_cosf(sunAngle), -SDL_sinf(sunAngle) - 0.30f);
            outLightDir = outLightDir.normalized();
        }
    }

void Game2DScene::updateSun() {
    World* world = App::instance().getWorld();
    if (!world || sunEntity_ == 0) return;

    TransformComponent* t = world->getComponent<TransformComponent>(sunEntity_);
    LightComponent* l = world->getComponent<LightComponent>(sunEntity_);
    if (!t || !l) return;

    l->color = cachedSunColor_;
    l->intensity = cachedSunIntensity_;

    // Directional lights have no meaningful position; keep it at the origin.
    t->transform.position = Vec3(0.0f, 0.0f, 0.0f);

    // Orient the transform so Transform::forward() matches the cached light
    // direction. forward() for an identity quaternion is +Z, so we rotate +Z
    // onto the (lightDir.x, lightDir.y, 0) vector.
    Vec3 lightTarget(cachedLightDir_.x, cachedLightDir_.y, 0.0f);
    Vec3 defaultForward(0.0f, 0.0f, 1.0f);
    Vec3 axis = Vec3::cross(defaultForward, lightTarget).normalized();
    if (axis.length() > 0.001f) {
        float angle = std::acos(Vec3::dot(defaultForward, lightTarget));
        float halfAngle = angle * 0.5f;
        float s = std::sin(halfAngle);
        t->transform.rotation = Quat(axis.x * s, axis.y * s, axis.z * s,
                                     std::cos(halfAngle));
    } else {
        // Light direction is already (or exactly opposite to) +Z; identity works.
        t->transform.rotation = Quat();
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
    MenuScene() {
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

    const char* name() const override { return "MenuScene"; }

    UIView* getUIRoot() override { return &menu_; }
    UIContext* getUIContext() override { return &ui_; }

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

    void render(Canvas2D* canvas) override {
        if (!canvas) return;

        // Background
        canvas->setFillColor(Color(0.08f, 0.08f, 0.12f));
        canvas->fillRect(0, 0, 1280, 720);

        // Title bar
        canvas->setFillColor(Color(0.9f, 0.9f, 0.9f));
        canvas->fillRect(340, 160, 600, 80);

        // Hint bars representing "Press 1 or click for 2D, Press 2 or click for 3D"
        canvas->setFillColor(Color(0.7f, 0.7f, 0.7f));
        canvas->fillRect(440, 560, 400, 6);
        canvas->fillRect(440, 580, 400, 6);
        canvas->fillRect(440, 600, 280, 6);
    }

private:
    UIContext ui_;
    UIView menu_;
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
