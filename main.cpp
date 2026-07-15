#include "domi/app.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "domi/script.h"
#include "domi/scene.h"
#include "domi/scene_manager.h"
#include "domi/math.h"
#include "domi/input.h"
#include "domi/render_layer.h"
#include "domi/render_list.h"
#include "domi/render_node.h"
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

// A 2D tree sprite with separate trunk and foliage materials.
// The trunk is sorted by the tree bottom; foliage draws slightly after it.
class TreeNode : public RenderNode {
public:
    const Material* trunk = nullptr;
    const Material* foliage = nullptr;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;

    void build(RenderList& list, RenderLayer layer) const override {
        const float treeHeight = 80.0f;
        float bottomZ = y + treeHeight * 0.5f;
        if (trunk && trunk->width > 0) {
            DrawBatch batch;
            batch.save();
            batch.translate(x, y + 40.0f);
            batch.scale(scale, scale);
            batch.drawMaterial(-trunk->width * 0.5f, -trunk->height, *trunk);
            batch.restore();
            list.submit(layer, bottomZ, batch);
        }
        if (foliage && foliage->width > 0) {
            DrawBatch batch;
            batch.save();
            batch.translate(x, y + 40.0f);
            batch.scale(scale, scale);
            batch.drawMaterial(-foliage->width * 0.5f, -foliage->height, *foliage);
            batch.restore();
            list.submit(layer, bottomZ + 0.1f, batch);
        }
    }
};

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

// Scene layout data: the single source of truth for object placement.
// buildRenderTree(), updateRenderNodes(), updateCloudShadowCasters() and
// drawGroundShadows() all read from these tables.
namespace {
struct CloudDef { float baseX; float y; float speed; }; // sort z = y
const CloudDef kClouds[] = {
    {  150.0f, 100.0f, 1.0f },
    {  450.0f,  80.0f, 0.7f },
    {  800.0f, 130.0f, 1.2f },
    {  250.0f, 170.0f, 0.5f },
    {  650.0f, 200.0f, 0.9f },
    { 1050.0f, 230.0f, 1.1f },
};
const int kCloudCount = sizeof(kClouds) / sizeof(kClouds[0]);

struct TreeDef { float x; float y; }; // sort z = trunk bottom (y + 40)
const TreeDef kTrees[] = {
    { 180.0f, 320.0f }, { 940.0f, 290.0f }, { 820.0f, 460.0f },
    { 220.0f, 560.0f }, { 840.0f, 600.0f },
};
const int kTreeCount = sizeof(kTrees) / sizeof(kTrees[0]);
} // namespace

class Game2DScene : public Scene {
public:
    static constexpr float kHorizonY = 240.0f;

    Game2DScene()
        : carT_(0.0f), carSpeed_(0.25f), cloudOffset_(0.0f),
          cubeAngleX_(0.0f), cubeAngleY_(0.0f), sunEntity_(0),
          useZBuffer_(true), dayTime_(6.0f), dayDuration_(120.0f),
          cachedSunColor_(1.0f, 0.95f, 0.85f, 1.0f),
          cachedSunIntensity_(1.0f),
          cachedLightDir_(0.0f, -1.0f),
          cachedOverlay_(1.0f, 1.0f, 1.0f, 0.0f),
          carNode_(nullptr), overlayNode_(nullptr) {}

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
        updateRenderNodes();

        if (App::instance().getInput()->isKeyPressed(SDL_SCANCODE_R)) {
            App::instance().getSceneManager()->setNext(new SecondScene());
        }

        if (App::instance().getInput()->isKeyPressed(SDL_SCANCODE_Z)) {
            useZBuffer_ = !useZBuffer_;
            fprintf(stderr, "[GAME2D] Render mode: %s\n",
                    useZBuffer_ ? "z-buffer" : "painter");
        }
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
    void buildRenderTree();
    void updateRenderNodes();
    Vec2 cloudPosition(int i) const;

    // Dynamic render nodes that are animated in update().
    std::vector<MaterialNode*> cloudNodes_;
    CustomNode* carNode_;
    RectNode* overlayNode_;

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

    void drawGroundShadows(DrawBatch& batch) {
        Vec2 shadowDir = cachedLightDir_ * -1.0f;
        if (shadowDir.y <= 0.0f) return;

        for (int i = 0; i < kTreeCount; ++i) {
            float s = perspectiveScale(kTrees[i].y);
            SceneFunction::drawShadow(batch, kTrees[i].x, kTrees[i].y + 40.0f,
                                      32.0f * s, 10.0f * s, 0.0f,
                                      shadowDir);
        }
        SceneFunction::drawShadow(batch, 560,  360, 48.0f, 16.0f, 45.0f, shadowDir);
        SceneFunction::drawShadow(batch, 1050, 520, 34.0f, 12.0f, 30.0f, shadowDir);
    }

    void drawCenterLine(DrawBatch& batch) {
        batch.setStrokeColor(Color(0.9f, 0.85f, 0.3f));
        batch.setLineWidth(4.0f);
        for (int i = 0; i < 12; ++i) {
            float t0 = i / 12.0f;
            float t1 = (i + 0.5f) / 12.0f;
            float x0 = -100 + t0 * 1480;
            float y0 = 330 + t0 * 200;
            float x1 = -100 + t1 * 1480;
            float y1 = 330 + t1 * 200;
            batch.beginPath();
            batch.moveTo(x0, y0);
            batch.lineTo(x1, y1);
            batch.stroke();
        }
    }

    void draw3DCar(DrawBatch& batch, float x, float y) {
        float time = (float)App::instance().getTime();
        float yaw = carT_ * 0.25f;
        float roll = SDL_sinf(time * 10.0f) * 0.03f;

        static const std::vector<Mesh3D> carMeshes = Car3DGenerator().build();

        for (const Mesh3D& mesh : carMeshes) {
            batch.drawMesh3D(x, y, 1.0f, 0.0f, 0.0f, yaw + roll, mesh);
        }
    }

    void drawCube3D(DrawBatch& batch, float cx, float cy, float size, float ax, float ay) {
        static const std::vector<Mesh3D> cubeFaces = Cube3DGenerator().setSize(1.0f).build();

        for (const Mesh3D& face : cubeFaces) {
            batch.drawMesh3D(cx, cy, size, ax, ay, 0.0f, face);
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
    for (int i = 0; i < kTreeCount; ++i) {
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
    buildRenderTree();
}

void Game2DScene::unload(World* world, ScriptSystem* script) {
    (void)script;
    setRootNode(nullptr);
    cloudNodes_.clear();
    carNode_ = nullptr;
    overlayNode_ = nullptr;
    if (world) world->clear();
    sunEntity_ = 0;
    cloudEntities_.clear();
}

void Game2DScene::buildRenderTree() {
    auto root = std::unique_ptr<GroupNode>(new GroupNode());

    // Background layer: horizon and clouds (clouds sort by their y).
    LayerView& background = root->backgroundLayer(
        MaterialNode(horizonMaterial_, 0.0f, 120.0f).setZ(kHorizonY)
    );
    cloudNodes_.clear();
    for (int i = 0; i < kCloudCount; ++i) {
        MaterialNode& node = background.addChild<MaterialNode>(
            kClouds[i].y, cloudMaterials_[i], 0.0f, 0.0f, true);
        cloudNodes_.push_back(&node);
    }

    // Ground layer: grass and lake.
    root->groundLayer(
        RectNode(0.0f, kHorizonY, 1280.0f, 480.0f,
                 Color(0.28f, 0.72f, 0.28f)).setZ(kHorizonY),
        PathNode().setZ(kHorizonY + 1.0f)
            .fillColor(Color(0.18f, 0.48f, 0.78f, 1.0f))
            .strokeColor(Color(0.45f, 0.72f, 0.95f, 1.0f))
            .lineWidth(3.0f)
            .moveTo(850.0f, 360.0f)
            .quadraticCurveTo(1050.0f, 330.0f, 1240.0f, 410.0f)
            .quadraticCurveTo(1300.0f, 540.0f, 1220.0f, 670.0f)
            .quadraticCurveTo(1020.0f, 710.0f, 860.0f, 640.0f)
            .quadraticCurveTo(790.0f, 510.0f, 850.0f, 360.0f)
            .closePath()
    );

    // Surface layer: road, center line, and ground-object shadows.
    root->surfaceLayer(
        PathNode().setZ(560.0f)
            .fillColor(Color(0.35f, 0.35f, 0.35f))
            .moveTo(-121.0f, 300.0f)
            .lineTo(-79.0f, 360.0f)
            .lineTo(1401.0f, 560.0f)
            .lineTo(1359.0f, 500.0f)
            .closePath(),
        CustomNode([this](DrawBatch& batch) { drawCenterLine(batch); }).setZ(561.0f),
        CustomNode([this](DrawBatch& batch) { drawGroundShadows(batch); }).setZ(562.0f)
    );

    // Object layer: trees, house, rocks, and the 3D car.
    // Sprites sort by their bottom edge; the car's z is animated per frame.
    LayerView& object = root->objectLayer();
    for (int i = 0; i < kTreeCount; ++i) {
        TreeNode& node = object.addChild<TreeNode>(0.0f);
        node.trunk = &treeTrunks_[i];
        node.foliage = &treeFoliages_[i];
        node.x = kTrees[i].x;
        node.y = kTrees[i].y;
        node.scale = perspectiveScale(kTrees[i].y);
    }
    object.add(
        MaterialNode(houseMaterial_, 560.0f, 360.0f, true).sortByBottom(),
        MaterialNode(rockMaterials_[0], 420.0f, 380.0f, true).sortByBottom(),
        MaterialNode(rockMaterials_[1], 460.0f, 400.0f, true).sortByBottom(),
        MaterialNode(rockMaterials_[2], 720.0f, 540.0f, true).sortByBottom(),
        MaterialNode(rockMaterials_[3], 760.0f, 560.0f, true).sortByBottom()
    );
    carNode_ = &object.addChild<CustomNode>(0.0f, [this](DrawBatch& batch) {
        float cx = -100 + carT_ * 1480;
        float cy = 330 + carT_ * 200;
        batch.setRenderMode(useZBuffer_ ? RenderMode::ZBUFFER : RenderMode::PAINTER);
        batch.begin3D();
        draw3DCar(batch, cx, cy);
        batch.end3D();
    });

    // Effect layer: 3D rotating cube.
    root->effectLayer(
        CustomNode([this](DrawBatch& batch) {
            batch.setRenderMode(useZBuffer_ ? RenderMode::ZBUFFER : RenderMode::PAINTER);
            batch.begin3D();
            drawCube3D(batch, 1050.0f, 520.0f, 60.0f, cubeAngleX_, cubeAngleY_);
            batch.end3D();
        }).setZ(550.0f)
    );

    // Overlay layer: day/night tint.
    LayerView& overlay = root->overlayLayer();
    overlayNode_ = &overlay.addChild<RectNode>(
        0.0f, 0.0f, 0.0f, 1280.0f, 720.0f, cachedOverlay_);

    setRootNode(std::move(root));
    updateRenderNodes();
}

Vec2 Game2DScene::cloudPosition(int i) const {
    return Vec2(kClouds[i].baseX + cloudOffset_ * kClouds[i].speed, kClouds[i].y);
}

void Game2DScene::updateRenderNodes() {
    // Cloud positions drift with cloudOffset_.
    for (int i = 0; i < kCloudCount && i < (int)cloudNodes_.size(); ++i) {
        Vec2 p = cloudPosition(i);
        cloudNodes_[i]->setPosition(p.x, p.y);
    }

    // The car's z follows its screen y so it sorts correctly with other objects.
    if (carNode_) {
        carNode_->z = 330 + carT_ * 200;
    }

    // Day/night tint.
    if (overlayNode_) {
        overlayNode_->setColor(cachedOverlay_);
    }
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

    const float horizonDrawY = 120.0f;
    const float cloudHalfH = 40.0f;
    const int skylineW = static_cast<int>(horizonSkyline_.size());

    // No ground shadows when the sun is below the horizon.
    bool sunAboveHorizon = (cachedLightDir_.y < 0.0f);

    for (int i = 0; i < kCloudCount; ++i) {
        Vec2 pos = cloudPosition(i);
        TransformComponent* t = world->getComponent<TransformComponent>(cloudEntities_[i]);
        SpriteComponent* s = world->getComponent<SpriteComponent>(cloudEntities_[i]);
        if (t) {
            t->transform.position.x = pos.x;
            t->transform.position.y = pos.y;
        }

        // A cloud casts a shadow only while the sun is up and the cloud is
        // clearly above the horizon hills.
        if (s && skylineW > 0) {
            int idx = static_cast<int>(pos.x);
            if (idx < 0) idx = 0;
            if (idx >= skylineW) idx = skylineW - 1;
            float cloudBottom = pos.y + cloudHalfH;
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

        auto root = std::unique_ptr<GroupNode>(new GroupNode());
        root->backgroundLayer(
            RectNode(0.0f, 0.0f, 1280.0f, 720.0f,
                     Color(0.08f, 0.08f, 0.12f)).setZ(0.0f),
            RectNode(340.0f, 160.0f, 600.0f, 80.0f,
                     Color(0.9f, 0.9f, 0.9f)).setZ(1.0f),
            RectNode(440.0f, 560.0f, 400.0f, 6.0f,
                     Color(0.7f, 0.7f, 0.7f)).setZ(2.0f),
            RectNode(440.0f, 580.0f, 400.0f, 6.0f,
                     Color(0.7f, 0.7f, 0.7f)).setZ(2.0f),
            RectNode(440.0f, 600.0f, 280.0f, 6.0f,
                     Color(0.7f, 0.7f, 0.7f)).setZ(2.0f)
        );
        setRootNode(std::move(root));
    }

    void unload(World* world, ScriptSystem* script) override {
        (void)world;
        (void)script;
        setRootNode(nullptr);
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
