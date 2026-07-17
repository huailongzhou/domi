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
#include "domi/material_io.h"
#include "second_scene.h"
#include "3d/cube3d_generator.h"
#include "3d/car3d_generator.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <SDL3/SDL_filesystem.h>

using namespace domi;

// Out-of-line definitions required by C++11 when these constants are ODR-used
// (e.g. bound to a forwarding reference in GroupNode::addChild).
constexpr float Game2DScene::kHorizonY;
constexpr float Game2DScene::kWorldWidth;
constexpr float Game2DScene::kWorldHeight;

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

// Derives the hill-top skyline from a baked horizon texture: per x column,
// the y of the first pixel whose alpha is above a small threshold (the sky
// part of the texture is transparent).
static std::vector<int> deriveSkyline(const Material& m) {
    std::vector<int> skyline(m.width, m.height);
    if (m.format != PixelFormat::ARGB8888) return skyline;
    for (int x = 0; x < m.width; ++x) {
        for (int y = 0; y < m.height; ++y) {
            const uint8_t a = m.pixels[((size_t)y * m.width + x) * 4];
            if (a > 8) {
                skyline[x] = y;
                break;
            }
        }
    }
    return skyline;
}

void Game2DScene::loadMaterials() {
    materials_.clear();

    // Load every baked texture in assets/materials/ (same prefix search as
    // the scene file). Material names come from the file names (<name>.tex),
    // so anything the editor exports is available to the scene.
    const char* prefixes[] = { "", "../", "../../" };
    for (size_t p = 0; p < sizeof(prefixes) / sizeof(prefixes[0]); ++p) {
        const std::string dir = std::string(prefixes[p]) + "assets/materials";
        int count = 0;
        char** files = SDL_GlobDirectory(dir.c_str(), "*.tex", 0, &count);
        if (!files) continue;
        for (int i = 0; i < count; ++i) {
            // Entries may or may not include the directory prefix.
            std::string path = files[i];
            std::string base = path;
            const size_t slash = base.find_last_of("/\\");
            if (slash != std::string::npos) {
                base = base.substr(slash + 1);
            } else {
                path = dir + "/" + path;
            }
            if (base.size() < 4 || base.compare(base.size() - 4, 4, ".tex") != 0) continue;
            Material m;
            if (!loadMaterialFile(path.c_str(), m)) {
                fprintf(stderr, "[GAME2D] Cannot load material '%s'\n", path.c_str());
                continue;
            }
            m.id = base.substr(0, base.size() - 4);
            materials_.push_back(m);
        }
        SDL_free(files);
        break; // first existing directory wins
    }

    materialMap_.clear();
    for (size_t i = 0; i < materials_.size(); ++i) {
        materialMap_[materials_[i].id] = &materials_[i];
    }
    fprintf(stderr, "[GAME2D] Loaded %d baked materials\n", (int)materials_.size());
}

void Game2DScene::load(World* world, ScriptSystem* script) {
    (void)script;

    loadMaterials();

    const std::map<std::string, const Material*>::const_iterator horizon =
        materialMap_.find("horizon");
    if (horizon != materialMap_.end()) {
        horizonSkyline_ = deriveSkyline(*horizon->second);
    } else {
        horizonSkyline_.clear();
    }

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
    carNode_ = nullptr;
    overlayNode_ = nullptr;
    if (world) world->clear();
    sunEntity_ = 0;
    cloudEntities_.clear();
}

void Game2DScene::buildRenderTree() {
    // Static layout (horizon, clouds, grass, lake, road, trees, houses,
    // rocks) comes from the scene file, built from standard node types only.
    // Materials are resolved by name from the map filled in load().
    SceneLoader loader;
    loader.setMaterialResolver([this](const std::string& name) -> const Material* {
        std::map<std::string, const Material*>::const_iterator it = materialMap_.find(name);
        if (it != materialMap_.end()) return it->second;
        fprintf(stderr, "[GAME2D] Unknown material '%s'\n", name.c_str());
        return nullptr;
    });

    clouds_.clear();
    cloudNodes_.clear();

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

    // Collect the scene's material nodes by role:
    // - clouds drift in update() and cast cloud shadows;
    // - trees/houses/rocks cast ground shadows from their node position, so
    //   moving them in the editor moves the shadow too;
    // - trees additionally get perspective-scaled by their ground y.
    propNodes_.clear();
    static const float kCloudSpeeds[] = { 1.0f, 0.7f, 1.2f, 0.5f, 0.9f, 1.1f };
    const std::vector<std::pair<std::string, MaterialNode*> >& mats =
        loader.materialNodes();
    for (size_t i = 0; i < mats.size(); ++i) {
        const std::string& name = mats[i].first;
        MaterialNode* node = mats[i].second;
        if (name.compare(0, 5, "cloud") == 0) {
            CloudDef def;
            def.baseX = node->getX();
            def.y = node->getY();
            const size_t cloudIndex = clouds_.size();
            def.speed = cloudIndex < sizeof(kCloudSpeeds) / sizeof(kCloudSpeeds[0])
                            ? kCloudSpeeds[cloudIndex] : 1.0f;
            clouds_.push_back(def);
            cloudNodes_.push_back(node);
        } else if (name.compare(0, 4, "tree") == 0) {
            node->setScale(perspectiveScale(node->getY()));
            propNodes_.push_back(node);
        } else if (name == "house" || name.compare(0, 4, "rock") == 0) {
            propNodes_.push_back(node);
        }
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

    // Trees, houses and rocks: shadow derived from the node's position,
    // material size and perspective scale (baseOffsetY reaches from the node
    // origin down to the sprite base).
    for (size_t i = 0; i < propNodes_.size(); ++i) {
        const MaterialNode* node = propNodes_[i];
        const Material* mat = node->getMaterial();
        if (!mat || mat->width <= 0) continue;
        const float s = node->getScale();
        const float w = static_cast<float>(mat->width) * s;
        const float h = static_cast<float>(mat->height); // unscaled: the
        // sprite base stays at the pivot regardless of scale
        const float cx = node->isCentered() ? node->getX() : node->getX() + w * 0.5f;
        const float cy = node->isCentered() ? node->getY() : node->getY() + h * 0.5f;
        SceneFunction::drawShadow(batch, cx, cy,
                                  w * 0.55f, w * 0.18f, h * 0.5f,
                                  shadowDir);
    }
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
