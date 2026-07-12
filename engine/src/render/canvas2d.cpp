#include "domi/canvas2d.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace domi {

namespace {

SDL_FColor toSDLColor(const Color& c) {
    SDL_FColor fc;
    fc.r = c.r;
    fc.g = c.g;
    fc.b = c.b;
    fc.a = c.a;
    return fc;
}

} // anonymous namespace

Canvas2D::Canvas2D(SDL_Renderer* renderer)
    : renderer_(renderer),
      target3D_(NULL),
      width_(1280),
      height_(720),
      lockedPixels_(NULL),
      lockedPitch_(0),
      in3D_(false),
      renderMode_(RenderMode::PAINTER),
      fillColor_(1, 1, 1, 1),
      strokeColor_(0, 0, 0, 1),
      lineWidth_(1.0f),
      pathClosed_(false) {
    if (renderer_) {
        SDL_GetRenderOutputSize(renderer_, &width_, &height_);
        target3D_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                      SDL_TEXTUREACCESS_STREAMING, width_, height_);
        if (target3D_) {
            SDL_SetTextureBlendMode(target3D_, SDL_BLENDMODE_BLEND);
        }
    }
    depthBuffer_.assign(width_ * height_, 1.0f);
}

Canvas2D::~Canvas2D() {
    if (target3D_) {
        SDL_DestroyTexture(target3D_);
    }
}

void Canvas2D::fillRect(float x, float y, float w, float h) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_,
        (uint8_t)(fillColor_.r * 255),
        (uint8_t)(fillColor_.g * 255),
        (uint8_t)(fillColor_.b * 255),
        (uint8_t)(fillColor_.a * 255));
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer_, &rect);
}

void Canvas2D::strokeRect(float x, float y, float w, float h) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_,
        (uint8_t)(strokeColor_.r * 255),
        (uint8_t)(strokeColor_.g * 255),
        (uint8_t)(strokeColor_.b * 255),
        (uint8_t)(strokeColor_.a * 255));
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderRect(renderer_, &rect);
}

void Canvas2D::clearRect(float x, float y, float w, float h) {
    // For SDL_Renderer, "clear" means overwrite with the current draw color.
    // We use the fill color as the clear color.
    fillRect(x, y, w, h);
}

void Canvas2D::drawLine(float x1, float y1, float x2, float y2) {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_,
        (uint8_t)(strokeColor_.r * 255),
        (uint8_t)(strokeColor_.g * 255),
        (uint8_t)(strokeColor_.b * 255),
        (uint8_t)(strokeColor_.a * 255));
    SDL_RenderLine(renderer_, x1, y1, x2, y2);
}

void Canvas2D::fillCircle(float x, float y, float radius, int segments) {
    if (!renderer_ || segments < 3) return;

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(segments + 2);
    std::vector<int> indices;
    indices.reserve(segments * 3);

    SDL_FColor col = toSDLColor(fillColor_);
    SDL_FPoint zero = { 0.0f, 0.0f };

    // Center vertex
    SDL_Vertex center;
    center.position.x = x;
    center.position.y = y;
    center.color = col;
    center.tex_coord = zero;
    vertices.push_back(center);

    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * 3.14159265f;
        SDL_Vertex v;
        v.position.x = x + radius * std::cos(angle);
        v.position.y = y + radius * std::sin(angle);
        v.color = col;
        v.tex_coord = zero;
        vertices.push_back(v);
    }

    for (int i = 0; i < segments; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }

    SDL_RenderGeometry(renderer_, NULL, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
}

void Canvas2D::beginPath() {
    path_.clear();
    pathClosed_ = false;
}

void Canvas2D::moveTo(float x, float y) {
    path_.push_back(Vec2(x, y));
}

void Canvas2D::lineTo(float x, float y) {
    path_.push_back(Vec2(x, y));
}

void Canvas2D::closePath() {
    pathClosed_ = true;
}

void Canvas2D::fill() {
    if (!renderer_ || path_.size() < 3) return;

    size_t n = path_.size();
    std::vector<SDL_Vertex> vertices;
    vertices.reserve(n);
    std::vector<int> indices;
    indices.reserve((n - 2) * 3);

    SDL_FColor col = toSDLColor(fillColor_);
    SDL_FPoint zero = { 0.0f, 0.0f };

    for (size_t i = 0; i < n; ++i) {
        SDL_Vertex v;
        v.position.x = path_[i].x;
        v.position.y = path_[i].y;
        v.color = col;
        v.tex_coord = zero;
        vertices.push_back(v);
    }

    for (size_t i = 1; i + 1 < n; ++i) {
        indices.push_back(0);
        indices.push_back((int)i);
        indices.push_back((int)(i + 1));
    }

    SDL_RenderGeometry(renderer_, NULL, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
}

void Canvas2D::stroke() {
    if (!renderer_ || path_.size() < 2) return;

    SDL_SetRenderDrawColor(renderer_,
        (uint8_t)(strokeColor_.r * 255),
        (uint8_t)(strokeColor_.g * 255),
        (uint8_t)(strokeColor_.b * 255),
        (uint8_t)(strokeColor_.a * 255));

    for (size_t i = 0; i + 1 < path_.size(); ++i) {
        SDL_RenderLine(renderer_, path_[i].x, path_[i].y, path_[i + 1].x, path_[i + 1].y);
    }

    if (pathClosed_ && path_.size() > 2) {
        SDL_RenderLine(renderer_, path_.back().x, path_.back().y, path_[0].x, path_[0].y);
    }
}

void Canvas2D::begin3D() {
    if (!renderer_ || !target3D_ || in3D_) return;
    if (SDL_LockTexture(target3D_, NULL, &lockedPixels_, &lockedPitch_)) {
        in3D_ = true;
        // Clear 3D layer to transparent.
        memset(lockedPixels_, 0, lockedPitch_ * height_);
        std::fill(depthBuffer_.begin(), depthBuffer_.end(), 1.0f);
    }
}

void Canvas2D::end3D() {
    if (!in3D_) return;
    in3D_ = false;
    SDL_UnlockTexture(target3D_);
    SDL_RenderTexture(renderer_, target3D_, NULL, NULL);
}

void Canvas2D::fillTriangle3D(const Vec2& a, const Vec2& b, const Vec2& c,
                              float za, float zb, float zc, const Color& color) {
    if (!in3D_ || !lockedPixels_) return;

    float minX = std::min(std::min(a.x, b.x), c.x);
    float minY = std::min(std::min(a.y, b.y), c.y);
    float maxX = std::max(std::max(a.x, b.x), c.x);
    float maxY = std::max(std::max(a.y, b.y), c.y);

    if (maxX < 0.0f || maxY < 0.0f || minX >= width_ || minY >= height_) return;
    if (minX < 0.0f) minX = 0.0f;
    if (minY < 0.0f) minY = 0.0f;
    if (maxX >= width_) maxX = width_ - 1;
    if (maxY >= height_) maxY = height_ - 1;

    Vec3 v0(c.x - a.x, c.y - a.y, 0.0f);
    Vec3 v1(b.x - a.x, b.y - a.y, 0.0f);
    float dot00 = Vec3::dot(v0, v0);
    float dot01 = Vec3::dot(v0, v1);
    float dot11 = Vec3::dot(v1, v1);
    float denom = dot00 * dot11 - dot01 * dot01;
    if (denom == 0.0f) return;

    uint8_t r = (uint8_t)(color.r * 255.0f);
    uint8_t g = (uint8_t)(color.g * 255.0f);
    uint8_t b = (uint8_t)(color.b * 255.0f);
    uint8_t a = (uint8_t)(color.a * 255.0f);

    for (int y = (int)minY; y <= (int)maxY; ++y) {
        uint8_t* row = (uint8_t*)lockedPixels_ + y * lockedPitch_;
        for (int x = (int)minX; x <= (int)maxX; ++x) {
            Vec3 v2((float)x - a.x, (float)y - a.y, 0.0f);
            float dot20 = Vec3::dot(v0, v2);
            float dot21 = Vec3::dot(v1, v2);
            float gamma = (dot11 * dot20 - dot01 * dot21) / denom;
            float beta  = (dot00 * dot21 - dot01 * dot20) / denom;
            float alpha = 1.0f - gamma - beta;
            if (alpha < 0.0f || beta < 0.0f || gamma < 0.0f) continue;

            float z = alpha * za + beta * zb + gamma * zc;
            int idx = y * width_ + x;
            if (z < depthBuffer_[idx]) {
                depthBuffer_[idx] = z;
                int px = x * 4;
                row[px + 0] = r;
                row[px + 1] = g;
                row[px + 2] = b;
                row[px + 3] = a;
            }
        }
    }
}

void Canvas2D::fillCube3D(float cx, float cy, float size, float rotX, float rotY, const Color& color) {
    fillBox3D(cx, cy, size, size, size, rotX, rotY, color);
}

void Canvas2D::fillBox3D(float cx, float cy, float sx, float sy, float sz,
                         float rotX, float rotY, const Color& color) {
    if (!in3D_) return;

    Vec3 v[8] = {
        Vec3(-0.5f * sx, -0.5f * sy, -0.5f * sz), Vec3( 0.5f * sx, -0.5f * sy, -0.5f * sz),
        Vec3( 0.5f * sx,  0.5f * sy, -0.5f * sz), Vec3(-0.5f * sx,  0.5f * sy, -0.5f * sz),
        Vec3(-0.5f * sx, -0.5f * sy,  0.5f * sz), Vec3( 0.5f * sx, -0.5f * sy,  0.5f * sz),
        Vec3( 0.5f * sx,  0.5f * sy,  0.5f * sz), Vec3(-0.5f * sx,  0.5f * sy,  0.5f * sz)
    };

    struct FaceDef { int i[4]; };
    FaceDef faces[6] = {
        { {0, 3, 2, 1} }, { {5, 6, 7, 4} }, { {3, 7, 6, 2} },
        { {4, 0, 1, 5} }, { {1, 2, 6, 5} }, { {4, 7, 3, 0} }
    };

    float cxr = SDL_cosf(rotX), sxr = SDL_sinf(rotX);
    float cyr = SDL_cosf(rotY), syr = SDL_sinf(rotY);

    auto rotate = [&](const Vec3& p) -> Vec3 {
        Vec3 r1(p.x, p.y * cxr - p.z * sxr, p.y * sxr + p.z * cxr);
        return Vec3(r1.x * cyr + r1.z * syr, r1.y, -r1.x * syr + r1.z * cyr);
    };

    const float cameraDist = 3.0f;
    const float projScale = 1.0f;
    auto project = [&](const Vec3& p) -> Vec2 {
        float denom = p.z + cameraDist;
        if (denom < 0.01f) denom = 0.01f;
        return Vec2(cx + p.x / denom * projScale, cy - p.y / denom * projScale);
    };

    Vec3 tv[8];
    for (int i = 0; i < 8; ++i) tv[i] = rotate(v[i]);

    for (int f = 0; f < 6; ++f) {
        const FaceDef& face = faces[f];
        Vec3 a = tv[face.i[0]];
        Vec3 b = tv[face.i[1]];
        Vec3 c_ = tv[face.i[2]];
        Vec3 n = Vec3::cross(b - a, c_ - a);
        if (n.z >= 0.0f) continue;

        float l = -n.z / n.length();
        if (l < 0.25f) l = 0.25f;
        if (l > 1.0f) l = 1.0f;
        Color shaded(color.r * l, color.g * l, color.b * l, color.a);

        Vec2 p0 = project(a);
        Vec2 p1 = project(b);
        Vec2 p2 = project(c_);
        Vec2 p3 = project(tv[face.i[3]]);

        fillTriangle3D(p0, p1, p2, a.z, b.z, c_.z, shaded);
        fillTriangle3D(p0, p2, p3, a.z, c_.z, tv[face.i[3]].z, shaded);
    }
}

} // namespace domi
