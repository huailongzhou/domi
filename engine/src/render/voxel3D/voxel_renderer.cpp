#include "voxel_renderer.h"
#include "domi/canvas2d.h"
#include "domi/component.h"
#include "domi/ecs.h"
#include <algorithm>
#include <cmath>

namespace domi {

namespace {

const float PI = 3.14159265f;

Mat4 buildViewProjection(const CameraComponent* camera,
                         const TransformComponent* cameraTransform,
                         int screenW, int screenH) {
    Mat4 view;
    Mat4 projection;

    if (camera && cameraTransform) {
        Vec3 eye = cameraTransform->transform.position;
        Vec3 target = eye + cameraTransform->transform.forward();
        Vec3 up = cameraTransform->transform.up();
        if (up.length() < 0.0001f) up = Vec3(0.0f, 1.0f, 0.0f);
        view = Mat4::lookAt(eye, target, up);

        float aspect = screenH > 0 ? (float)screenW / (float)screenH : 1.0f;
        if (camera->isPerspective) {
            projection = Mat4::perspective(camera->fov, aspect,
                                           camera->nearPlane, camera->farPlane);
        } else {
            float halfH = camera->size;
            float halfW = halfH * aspect;
            projection = Mat4::ortho(-halfW, halfW, -halfH, halfH,
                                     camera->nearPlane, camera->farPlane);
        }
    } else {
        // Default orthographic camera: looking at the origin from (5,5,5).
        view = Mat4::lookAt(Vec3(5.0f, 5.5f, 5.0f),
                            Vec3(0.0f, 0.0f, 0.0f),
                            Vec3(0.0f, 1.0f, 0.0f));
        float halfH = 5.0f;
        float halfW = halfH * (screenH > 0 ? (float)screenW / (float)screenH : 1.0f);
        projection = Mat4::ortho(-halfW, halfW, -halfH, halfH, 0.1f, 100.0f);
    }

    // NOTE: this engine's Mat4 multiplication order is reversed relative to
    // standard math notation: `A * B` here means `B * A` in the usual sense.
    Mat4 vp = view * projection;
    return vp;
}

// Face corner definitions in local voxel space. Corners are ordered so that
// when viewed from outside the face the screen-space signed area is positive
// (after the y-axis flip from NDC to screen coordinates).
struct FaceDef {
    Vec3 normal;
    Vec3 corners[4];
};

const FaceDef kFaces[6] = {
    { Vec3( 1.0f,  0.0f,  0.0f), { Vec3(1,0,0), Vec3(1,0,1), Vec3(1,1,1), Vec3(1,1,0) } }, // +X
    { Vec3(-1.0f,  0.0f,  0.0f), { Vec3(0,0,0), Vec3(0,1,0), Vec3(0,1,1), Vec3(0,0,1) } }, // -X
    { Vec3( 0.0f,  1.0f,  0.0f), { Vec3(0,1,0), Vec3(1,1,0), Vec3(1,1,1), Vec3(0,1,1) } }, // +Y
    { Vec3( 0.0f, -1.0f,  0.0f), { Vec3(0,0,0), Vec3(0,0,1), Vec3(1,0,1), Vec3(1,0,0) } }, // -Y
    { Vec3( 0.0f,  0.0f,  1.0f), { Vec3(0,0,1), Vec3(1,0,1), Vec3(1,1,1), Vec3(0,1,1) } }, // +Z
    { Vec3( 0.0f,  0.0f, -1.0f), { Vec3(0,0,0), Vec3(0,1,0), Vec3(1,1,0), Vec3(1,0,0) } }, // -Z
};

Color applyLighting(const Color& base, const Vec3& normal) {
    Vec3 lightDir = Vec3(0.5f, -1.0f, -0.5f).normalized();
    float diff = -Vec3::dot(normal, lightDir);
    if (diff < 0.0f) diff = 0.0f;
    float l = 0.5f + 0.5f * diff;
    return Color(base.r * l, base.g * l, base.b * l, base.a);
}

} // anonymous namespace

// Snap screen-space coordinates to a sub-pixel grid so that shared edges
// between adjacent voxel faces become bit-exact. This avoids the 1-pixel
// background seams (and the color bleeding of expansion) caused by separate
// SDL_RenderGeometry draw calls.
static float snapScreen(float v, float grid) {
    return std::round(v / grid) * grid;
}

VoxelRenderer::VoxelRenderer(Canvas2D* canvas)
    : canvas_(canvas) {}

void VoxelRenderer::render(World* world, const CameraComponent* camera,
                           const TransformComponent* cameraTransform,
                           int screenW, int screenH) {
    if (!canvas_ || !world) return;

    Mat4 viewProj = buildViewProjection(camera, cameraTransform, screenW, screenH);

    std::vector<Entity> entities = world->queryEntitiesWith(
        ComponentTypeMask().withTransform().withVoxel());

    // Camera forward direction in world space (approximate backface test).
    Vec3 viewDir(0.0f, 0.0f, -1.0f);
    if (cameraTransform) viewDir = cameraTransform->transform.forward();

    std::vector<Face> faces;
    faces.reserve(entities.size() * 64);

    for (size_t i = 0; i < entities.size(); ++i) {
        renderChunk(world, entities[i], viewProj, viewDir, cameraTransform,
                    screenW, screenH, faces);
    }

    if (faces.empty()) return;

    // Painter's algorithm: draw farther cubes first. Each face stores the
    // depth of its parent cube, so all faces of one cube are drawn together.
    std::stable_sort(faces.begin(), faces.end(),
                     [](const Face& a, const Face& b) { return a.z > b.z; });

    for (size_t i = 0; i < faces.size(); ++i) {
        const Face& f = faces[i];
        canvas_->fillQuad3D(f.screen[0], f.screen[1], f.screen[2], f.screen[3],
                            f.z, f.color);
    }
}

void VoxelRenderer::renderChunk(World* world, Entity entity,
                                const Mat4& viewProj,
                                const Vec3& viewDir,
                                const TransformComponent* cameraTransform,
                                int screenW, int screenH,
                                std::vector<Face>& faces) {
    (void)screenW; (void)screenH;

    TransformComponent* tc = world->getComponent<TransformComponent>(entity);
    VoxelComponent* vc = world->getComponent<VoxelComponent>(entity);
    if (!tc || !vc) return;

    const int w = vc->width;
    const int h = vc->height;
    const int d = vc->depth;
    if (w <= 0 || h <= 0 || d <= 0 || vc->voxels.empty()) return;

    Mat4 model = tc->transform.toMatrix();
    // NOTE: reversed multiplication order, see buildViewProjection above.
    Mat4 mvp = model * viewProj;

    auto getVoxel = [&](int x, int y, int z) -> uint8_t {
        if (x < 0 || x >= w || y < 0 || y >= h || z < 0 || z >= d) return 0;
        return vc->voxels[(z * h + y) * w + x];
    };

    auto getColor = [&](uint8_t index) -> Color {
        if (index == 0 || index >= vc->palette.size()) return Color(1, 1, 1, 1);
        return vc->palette[index];
    };

    for (int f = 0; f < 6; ++f) {
        const FaceDef& face = kFaces[f];
        const int nx = (int)face.normal.x;
        const int ny = (int)face.normal.y;
        const int nz = (int)face.normal.z;

        std::vector<uint8_t> visited(w * h * d, 0);
        auto at = [&](int xx, int yy, int zz) -> int { return (zz * h + yy) * w + xx; };

        for (int z = 0; z < d; ++z) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    uint8_t idx = getVoxel(x, y, z);
                    if (idx == 0) continue;
                    if (getVoxel(x + nx, y + ny, z + nz) != 0) continue;
                    if (visited[at(x, y, z)]) continue;

                    Color baseColor = getColor(idx);

                    // Backface culling for this face direction. The merged
                    // rectangle is planar, so one test is enough.
                    Vec4 normalClip = model * Vec4(face.normal.x,
                                                   face.normal.y,
                                                   face.normal.z, 0.0f);
                    Vec3 normalWorld(normalClip.x, normalClip.y, normalClip.z);
                    normalWorld = normalWorld.normalized();

                    Vec3 toCamera = viewDir;
                    if (cameraTransform) {
                        Vec4 centerWorld4 = model * Vec4(x + 0.5f, y + 0.5f,
                                                         z + 0.5f, 1.0f);
                        Vec3 centerWorld(centerWorld4.x, centerWorld4.y,
                                         centerWorld4.z);
                        toCamera = (cameraTransform->transform.position - centerWorld);
                        if (toCamera.length() > 0.0001f)
                            toCamera = toCamera.normalized();
                    }
                    if (Vec3::dot(normalWorld, toCamera) <= 0.0f) continue;

                    // Greedy merge: find the largest axis-aligned rectangle of
                    // visible, same-material faces on this plane.
                    int dx = 1, dy = 1, dz = 1;

                    auto canExtendY = [&](int yy) -> bool {
                        if (yy < 0 || yy >= h) return false;
                        for (int zz = z; zz < z + dz; ++zz) {
                            if (getVoxel(x, yy, zz) != idx) return false;
                            if (getVoxel(x + nx, yy + ny, zz + nz) != 0) return false;
                            if (visited[at(x, yy, zz)]) return false;
                        }
                        return true;
                    };

                    auto canExtendZ = [&](int zz) -> bool {
                        if (zz < 0 || zz >= d) return false;
                        for (int yy = y; yy < y + dy; ++yy) {
                            if (getVoxel(x, yy, zz) != idx) return false;
                            if (getVoxel(x + nx, yy + ny, zz + nz) != 0) return false;
                            if (visited[at(x, yy, zz)]) return false;
                        }
                        return true;
                    };

                    auto canExtendX = [&](int xx) -> bool {
                        if (xx < 0 || xx >= w) return false;
                        for (int zz = z; zz < z + dz; ++zz) {
                            for (int yy = y; yy < y + dy; ++yy) {
                                if (getVoxel(xx, yy, zz) != idx) return false;
                                if (getVoxel(xx + nx, yy + ny, zz + nz) != 0) return false;
                                if (visited[at(xx, yy, zz)]) return false;
                            }
                        }
                        return true;
                    };

                    if (ny == 0 && nz == 0) { // +/- X: expand in Y, then Z
                        while (y + dy < h && canExtendY(y + dy)) ++dy;
                        while (z + dz < d && canExtendZ(z + dz)) ++dz;
                    } else if (nx == 0 && nz == 0) { // +/- Y: expand in X, then Z
                        while (x + dx < w && canExtendX(x + dx)) ++dx;
                        while (z + dz < d && canExtendZ(z + dz)) ++dz;
                    } else { // +/- Z: expand in X, then Y
                        while (x + dx < w && canExtendX(x + dx)) ++dx;
                        while (y + dy < h && canExtendY(y + dy)) ++dy;
                    }

                    // Mark all covered voxel faces as visited.
                    for (int zz = z; zz < z + dz; ++zz)
                        for (int yy = y; yy < y + dy; ++yy)
                            for (int xx = x; xx < x + dx; ++xx)
                                visited[at(xx, yy, zz)] = 1;

                    // Build the merged face corners in local voxel space.
                    Vec3 local[4];
                    if (nx == 1) { // +X
                        local[0] = Vec3(x + 1, y, z);
                        local[1] = Vec3(x + 1, y, z + dz);
                        local[2] = Vec3(x + 1, y + dy, z + dz);
                        local[3] = Vec3(x + 1, y + dy, z);
                    } else if (nx == -1) { // -X
                        local[0] = Vec3(x, y, z);
                        local[1] = Vec3(x, y + dy, z);
                        local[2] = Vec3(x, y + dy, z + dz);
                        local[3] = Vec3(x, y, z + dz);
                    } else if (ny == 1) { // +Y
                        local[0] = Vec3(x, y + 1, z);
                        local[1] = Vec3(x + dx, y + 1, z);
                        local[2] = Vec3(x + dx, y + 1, z + dz);
                        local[3] = Vec3(x, y + 1, z + dz);
                    } else if (ny == -1) { // -Y
                        local[0] = Vec3(x, y, z);
                        local[1] = Vec3(x, y, z + dz);
                        local[2] = Vec3(x + dx, y, z + dz);
                        local[3] = Vec3(x + dx, y, z);
                    } else if (nz == 1) { // +Z
                        local[0] = Vec3(x, y, z + 1);
                        local[1] = Vec3(x + dx, y, z + 1);
                        local[2] = Vec3(x + dx, y + dy, z + 1);
                        local[3] = Vec3(x, y + dy, z + 1);
                    } else { // -Z
                        local[0] = Vec3(x, y, z);
                        local[1] = Vec3(x, y + dy, z);
                        local[2] = Vec3(x + dx, y + dy, z);
                        local[3] = Vec3(x + dx, y, z);
                    }

                    // Project the merged corners and use the rectangle center
                    // for painter's-algorithm depth sorting.
                    Vec2 screen[4];
                    bool allInFront = true;
                    for (int c = 0; c < 4; ++c) {
                        Vec4 clip = mvp * Vec4(local[c].x, local[c].y, local[c].z, 1.0f);
                        float iw = clip.w != 0.0f ? 1.0f / clip.w : 1.0f;
                        screen[c].x = (clip.x * iw * 0.5f + 0.5f) * screenW;
                        screen[c].y = (0.5f - clip.y * iw * 0.5f) * screenH;
                        if (clip.w < 0.0f) allInFront = false;
                    }
                    if (!allInFront) continue;

                    // Snap projected corners to a 1/4-pixel grid. Shared edges
                    // from separate draw calls then become bit-exact and the GPU
                    // rasterizer fills the same pixels for both faces.
                    const float kSnapGrid = 0.25f;
                    for (int c = 0; c < 4; ++c) {
                        screen[c].x = snapScreen(screen[c].x, kSnapGrid);
                        screen[c].y = snapScreen(screen[c].y, kSnapGrid);
                    }

                    Vec3 centerLocal = (local[0] + local[1] + local[2] + local[3]) * 0.25f;
                    Vec4 centerClip = mvp * Vec4(centerLocal.x, centerLocal.y, centerLocal.z, 1.0f);
                    float iw = centerClip.w != 0.0f ? 1.0f / centerClip.w : 1.0f;
                    float quadDepth = centerClip.z * iw;

                    Face out;
                    out.screen[0] = screen[0];
                    out.screen[1] = screen[1];
                    out.screen[2] = screen[2];
                    out.screen[3] = screen[3];
                    out.z = quadDepth;
                    out.color = applyLighting(baseColor, face.normal);
                    faces.push_back(out);
                }
            }
        }
    }
}

} // namespace domi
