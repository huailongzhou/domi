#include "game2d_scene.h"

#include "domi/app.h"
#include "domi/ecs.h"
#include "domi/component.h"
#include "domi/input.h"
#include "domi/scene_manager.h"
#include "domi/render_layer.h"
#include "domi/render_list.h"
#include "domi/render_node.h"
#include "domi/scene_loader.h"
#include "domi/scene_function.h"
#include "second_scene.h"
#include "tree_node.h"
#include "tree_generator.h"
#include "cloud_generator.h"
#include "rock_generator.h"
#include "house_generator.h"
#include "horizon_generator.h"
#include "3d/cube3d_generator.h"
#include "3d/car3d_generator.h"
#include <cmath>
#include <cstdio>

using namespace domi;

// Out-of-line definitions required by C++11 when these constants are ODR-used
// (e.g. bound to a forwarding reference in GroupNode::addChild).
constexpr float Game2DScene::kHorizonY;
constexpr float Game2DScene::kWorldWidth;
constexpr float Game2DScene::kWorldHeight;

namespace {

// How many cloud/tree sprite variants to pre-generate. Placements (which
// variant goes where) live in assets/scenes/game2d.json.
const int kCloudMaterialCount = 6;
const int kTreeMaterialCount = 10;

} // namespace

Game2DScene::Game2DScene()
    : carT_(0.0f), carSpeed_(0.25f), cloudOffset_(0.0f),
      cubeAngleX_(0.0f), cubeAngleY_(0.0f), sunEntity_(0),
      useZBuffer_(true), dayTime_(6.0f), dayDuration_(120.0f),
      cachedSunColor_(1.0f, 0.95f, 0.85f, 1.0f),
      cachedSunIntensity_(1.0f),
      cachedLightDir_(0.0f, -1.0f),
      cachedOverlay_(1.0f, 1.0f, 1.0f, 0.0f),
      carNode_(nullptr), overlayNode_(nullptr) {}

void Game2DScene::update(double dt) {
    updateCamera();

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

// Mouse-driven viewport control: left-drag pans, wheel zooms toward the
// cursor, Home resets. Transform convention: screen = world * zoom + offset.
// The viewport is clamped so it never leaves the world bounds.
void Game2DScene::updateCamera() {
    InputSystem* input = App::instance().getInput();
    if (!input) return;

    // Left-drag pans the viewport (screen-space, 1:1).
    if (input->isMouseButtonDown(1)) {
        camera_.offsetX += input->getMouseDeltaX();
        camera_.offsetY += input->getMouseDeltaY();
    }

    // Mouse wheel zooms toward the cursor position: keep the world point
    // under the cursor fixed, i.e. offset' = m - (m - offset) * (zoom'/zoom).
    float scroll = input->getScrollY();
    if (scroll != 0.0f) {
        // Never zoom out further than "world exactly fills the window".
        float minZoom = 1280.0f / kWorldWidth;
        if (720.0f / kWorldHeight > minZoom) minZoom = 720.0f / kWorldHeight;

        const float oldZoom = camera_.zoom;
        float newZoom = oldZoom * std::pow(1.15f, scroll);
        if (newZoom < minZoom) newZoom = minZoom;
        if (newZoom > 4.0f) newZoom = 4.0f;

        float mx = input->getMouseX();
        float my = input->getMouseY();
        camera_.offsetX = mx - (mx - camera_.offsetX) * (newZoom / oldZoom);
        camera_.offsetY = my - (my - camera_.offsetY) * (newZoom / oldZoom);
        camera_.zoom = newZoom;
    }

    // Home resets the viewport.
    if (input->isKeyPressed(SDL_SCANCODE_HOME)) {
        camera_ = Camera2D();
    }

    clampCamera();
}

// Keep the viewport inside the world:
//   visible world rect = [(0-offset)/zoom, (screen-offset)/zoom]
// must stay within [0, kWorldWidth] x [0, kWorldHeight]. When the world is
// smaller than the window at the current zoom, center it instead.
void Game2DScene::clampCamera() {
    const float screenW = 1280.0f;
    const float screenH = 720.0f;
    const float viewW = kWorldWidth * camera_.zoom;
    const float viewH = kWorldHeight * camera_.zoom;

    if (viewW >= screenW) {
        if (camera_.offsetX > 0.0f) camera_.offsetX = 0.0f;
        if (camera_.offsetX < screenW - viewW) camera_.offsetX = screenW - viewW;
    } else {
        camera_.offsetX = (screenW - viewW) * 0.5f;
    }

    if (viewH >= screenH) {
        if (camera_.offsetY > 0.0f) camera_.offsetY = 0.0f;
        if (camera_.offsetY < screenH - viewH) camera_.offsetY = screenH - viewH;
    } else {
        camera_.offsetY = (screenH - viewH) * 0.5f;
    }
}

void Game2DScene::load(World* world, ScriptSystem* script) {
    (void)script;

    // Generate tree and cloud materials using the fluent generator API.
    // Format can be changed to PixelFormat::RGB565, PixelFormat::LUT8,
    // or PixelFormat::AlphaMask to experiment with different outputs.
    // Generate several randomized variants for each material type so that the
    // scene looks varied instead of repeating the same sprite.
    treeTrunks_.clear();
    treeFoliages_.clear();
    for (int i = 0; i < kTreeMaterialCount; ++i) {
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
    for (int i = 0; i < kCloudMaterialCount; ++i) {
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
    horizonGen.setSize(static_cast<int>(kWorldWidth), 120)
        .setFormat(PixelFormat::ARGB8888)
        .setSeed(5000u)
        .setBaseColor(Color(0.28f, 0.72f, 0.28f, 1.0f))
        .setSkyColor(Color(0.12f, 0.12f, 0.16f, 1.0f))
        .setHillColor(Color(0.15f, 0.40f, 0.15f, 1.0f))
        .setHillCount(5)
        .setHillHeight(25, 70);
    horizonMaterial_ = horizonGen.build();
    horizonSkyline_ = horizonGen.buildSkyline();

    // buildRenderTree() fills the cloud layout from the scene file, which
    // createShadowCasters() depends on — keep this order.
    buildRenderTree();
    createShadowCasters(world);
}

void Game2DScene::unload(World* world, ScriptSystem* script) {
    (void)script;
    setRootNode(nullptr);
    clouds_.clear();
    cloudNodes_.clear();
    treeNodes_.clear();
    carNode_ = nullptr;
    overlayNode_ = nullptr;
    if (world) world->clear();
    sunEntity_ = 0;
    cloudEntities_.clear();
}

void Game2DScene::buildRenderTree() {
    // Static layout (horizon, clouds, grass, lake, road, trees, houses,
    // rocks) comes from the scene file. Materials are resolved by name from
    // the vectors generated in load().
    SceneLoader loader;
    loader.setMaterialResolver([this](const std::string& name) -> const Material* {
        if (name == "horizon") return &horizonMaterial_;
        if (name == "house")   return &houseMaterial_;
        int idx = -1;
        if (sscanf(name.c_str(), "cloud%d", &idx) == 1 &&
            idx >= 0 && idx < (int)cloudMaterials_.size()) return &cloudMaterials_[idx];
        if (sscanf(name.c_str(), "trunk%d", &idx) == 1 &&
            idx >= 0 && idx < (int)treeTrunks_.size()) return &treeTrunks_[idx];
        if (sscanf(name.c_str(), "foliage%d", &idx) == 1 &&
            idx >= 0 && idx < (int)treeFoliages_.size()) return &treeFoliages_[idx];
        if (sscanf(name.c_str(), "rock%d", &idx) == 1 &&
            idx >= 0 && idx < (int)rockMaterials_.size()) return &rockMaterials_[idx];
        fprintf(stderr, "[GAME2D] Unknown material '%s'\n", name.c_str());
        return nullptr;
    });
    // Demo node types registered with the loader:
    // - "cloud": drifts along a fixed y with its own speed; also registers a
    //   shadow caster slot and an animation slot.
    loader.registerType("cloud", [this](const nlohmann::json& j, const SceneLoader& l)
            -> std::unique_ptr<RenderNode> {
        const Material* material = l.resolveMaterial(j.value("material", ""));
        if (!material) return nullptr;
        CloudDef def;
        def.baseX = j.value("x", 0.0f);
        def.y     = j.value("y", 0.0f);
        def.speed = j.value("speed", 1.0f);
        clouds_.push_back(def);
        std::unique_ptr<MaterialNode> node(
            new MaterialNode(*material, def.baseX, def.y, true));
        node->sortByCenterY();
        cloudNodes_.push_back(node.get());
        return node;
    });
    // - "tree": trunk + foliage sprite pair, scaled by perspective.
    loader.registerType("tree", [this](const nlohmann::json& j, const SceneLoader& l)
            -> std::unique_ptr<RenderNode> {
        std::unique_ptr<TreeNode> node(new TreeNode());
        node->trunk   = l.resolveMaterial(j.value("trunk", ""));
        node->foliage = l.resolveMaterial(j.value("foliage", ""));
        node->x = j.value("x", 0.0f);
        node->y = j.value("y", 0.0f);
        node->scale = perspectiveScale(node->y);
        treeNodes_.push_back(node.get());
        return node;
    });

    clouds_.clear();
    cloudNodes_.clear();
    treeNodes_.clear();

    // Locate the scene file regardless of the directory the game was
    // started from (project root, build/, ...).
    const char* kSceneCandidates[] = {
        "assets/scenes/game2d.json",
        "../assets/scenes/game2d.json",
        "../../assets/scenes/game2d.json",
    };
    std::unique_ptr<GroupNode> root;
    for (size_t i = 0; i < sizeof(kSceneCandidates) / sizeof(kSceneCandidates[0]); ++i) {
        FILE* f = fopen(kSceneCandidates[i], "r");
        if (!f) continue;
        fclose(f);
        root = loader.load(kSceneCandidates[i]);
        break;
    }
    if (!root) {
        fprintf(stderr, "[GAME2D] Scene file missing, using an empty layout\n");
        root.reset(new GroupNode());
    }

    // Dynamic nodes below are attached in code.

    // Surface layer: ground-object shadows (explicit z — a multi-caster batch
    // has no intrinsic y range) and the dashed yellow center line.
    LayerView& surface = root->surfaceLayer();
    CustomNode& shadows = surface.addChild<CustomNode>(
        [this](DrawBatch& batch) { drawGroundShadows(batch); });
    shadows.z = 562.0f;
    const int dashCount = 24;
    for (int i = 0; i < dashCount; ++i) {
        float t0 = i / (float)dashCount;
        float t1 = (i + 0.5f) / (float)dashCount;
        surface.addChild<LineNode>(
            -100 + t0 * (kWorldWidth + 200.0f), 330 + t0 * 400.0f,
            -100 + t1 * (kWorldWidth + 200.0f), 330 + t1 * 400.0f,
            Color(0.9f, 0.85f, 0.3f)).lineWidth(4.0f).sortByBottom();
    }

    // Object layer: the 3D car (its z is animated per frame).
    carNode_ = &root->objectLayer().addChild<CustomNode>([this](DrawBatch& batch) {
        float cx = -100 + carT_ * (kWorldWidth + 200.0f);
        float cy = 330 + carT_ * 400.0f;
        batch.setRenderMode(useZBuffer_ ? RenderMode::ZBUFFER : RenderMode::PAINTER);
        batch.begin3D();
        draw3DCar(batch, cx, cy);
        batch.end3D();
    });

    // Effect layer: 3D rotating cube.
    root->effectLayer().addChild<CustomNode>([this](DrawBatch& batch) {
        batch.setRenderMode(useZBuffer_ ? RenderMode::ZBUFFER : RenderMode::PAINTER);
        batch.begin3D();
        drawCube3D(batch, 1050.0f, 520.0f, 60.0f, cubeAngleX_, cubeAngleY_);
        batch.end3D();
    });

    // Overlay layer: day/night tint.
    overlayNode_ = &root->overlayLayer().addChild<RectNode>(
        0.0f, 0.0f, 1280.0f, 720.0f, cachedOverlay_);

    setRootNode(std::move(root));
    updateRenderNodes();
}

Vec2 Game2DScene::cloudPosition(int i) const {
    return Vec2(clouds_[i].baseX + cloudOffset_ * clouds_[i].speed, clouds_[i].y);
}

void Game2DScene::updateRenderNodes() {
    // Cloud positions drift with cloudOffset_.
    for (int i = 0; i < (int)clouds_.size() && i < (int)cloudNodes_.size(); ++i) {
        Vec2 p = cloudPosition(i);
        cloudNodes_[i]->setPosition(p.x, p.y);
    }

    // The car's z follows its screen y so it sorts correctly with other objects.
    if (carNode_) {
        carNode_->z = 330 + carT_ * 400.0f;
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
    for (int i = 0; i < (int)clouds_.size(); ++i) {
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
    if (!world || cloudEntities_.size() < clouds_.size()) return;

    const float horizonDrawY = 120.0f;
    const float cloudHalfH = 40.0f;
    const int skylineW = static_cast<int>(horizonSkyline_.size());

    // No ground shadows when the sun is below the horizon.
    bool sunAboveHorizon = (cachedLightDir_.y < 0.0f);

    for (int i = 0; i < (int)clouds_.size(); ++i) {
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

float Game2DScene::perspectiveScale(float y) const {
    // Objects near the horizon (y=240) are far away and smaller;
    // objects near the bottom (y=720) are close and full size.
    const float minScale = 0.35f;
    const float maxScale = 1.0f;
    float t = (y - kHorizonY) / (720.0f - kHorizonY);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return minScale + t * (maxScale - minScale);
}

void Game2DScene::drawGroundShadows(DrawBatch& batch) {
    Vec2 shadowDir = cachedLightDir_ * -1.0f;
    if (shadowDir.y <= 0.0f) return;

    for (size_t i = 0; i < treeNodes_.size(); ++i) {
        const TreeNode* tree = static_cast<const TreeNode*>(treeNodes_[i]);
        float s = perspectiveScale(tree->y);
        SceneFunction::drawShadow(batch, tree->x, tree->y + 40.0f,
                                  32.0f * s, 10.0f * s, 0.0f,
                                  shadowDir);
    }
    SceneFunction::drawShadow(batch,  560,  360, 48.0f, 16.0f, 45.0f, shadowDir);
    SceneFunction::drawShadow(batch, 2100,  480, 48.0f, 16.0f, 45.0f, shadowDir);
    SceneFunction::drawShadow(batch, 1050,  520, 34.0f, 12.0f, 30.0f, shadowDir);
}

void Game2DScene::draw3DCar(DrawBatch& batch, float x, float y) {
    float time = (float)App::instance().getTime();
    float yaw = carT_ * 0.25f;
    float roll = SDL_sinf(time * 10.0f) * 0.03f;

    static const std::vector<Mesh3D> carMeshes = Car3DGenerator().build();

    for (const Mesh3D& mesh : carMeshes) {
        batch.drawMesh3D(x, y, 1.0f, 0.0f, 0.0f, yaw + roll, mesh);
    }
}

void Game2DScene::drawCube3D(DrawBatch& batch, float cx, float cy, float size, float ax, float ay) {
    static const std::vector<Mesh3D> cubeFaces = Cube3DGenerator().setSize(1.0f).build();

    for (const Mesh3D& face : cubeFaces) {
        batch.drawMesh3D(cx, cy, size, ax, ay, 0.0f, face);
    }
}
