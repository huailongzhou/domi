#include "domi/canvas2d.h"
#include "domi/backend/render_backend.h"
#include "domi/render_texture.h"
#include <cmath>
#include <algorithm>

namespace domi {

Canvas2D::Canvas2D(IRenderBackend* backend)
    : backend_(backend),
      currentTarget_(NULL),
      width_(backend ? backend->getWidth() : 1280),
      height_(backend ? backend->getHeight() : 720),
      in3D_(false),
      lockedPixels_(NULL),
      lockedPitch_(0),
      renderMode_(RenderMode::PAINTER),
      batching_(true),
      fillColor_(1, 1, 1, 1),
      strokeColor_(0, 0, 0, 1),
      lineWidth_(1.0f),
      pathClosed_(false) {
    depthBuffer_.assign(width_ * height_, 1.0f);
}

Canvas2D::~Canvas2D() {
}

void Canvas2D::setBatching(bool enabled) {
    if (!enabled && batching_) {
        flush();
    }
    batching_ = enabled;
}

void Canvas2D::flush() {
    if (backend_) {
        queue_.flush(backend_);
    } else {
        queue_.clear();
    }
}

void Canvas2D::setFillColor(const Color& c) {
    fillColor_ = c;
    if (batching_) {
        queue_.setFillColor(c);
    }
}

void Canvas2D::setStrokeColor(const Color& c) {
    strokeColor_ = c;
    if (batching_) {
        queue_.setStrokeColor(c);
    }
}

void Canvas2D::setLineWidth(float w) {
    lineWidth_ = w;
    if (batching_) {
        queue_.setLineWidth(w);
    }
}

void Canvas2D::fillRect(float x, float y, float w, float h) {
    if (!backend_) return;
    if (batching_) {
        queue_.fillRect(x, y, w, h);
        return;
    }
    backend_->setFillColor(fillColor_);
    backend_->fillRect(x, y, w, h);
}

void Canvas2D::strokeRect(float x, float y, float w, float h) {
    if (!backend_) return;
    if (batching_) {
        queue_.strokeRect(x, y, w, h);
        return;
    }
    backend_->setStrokeColor(strokeColor_);
    backend_->strokeRect(x, y, w, h);
}

void Canvas2D::clearRect(float x, float y, float w, float h) {
    if (batching_) {
        queue_.clearRect(x, y, w, h);
    } else {
        fillRect(x, y, w, h);
    }
}

void Canvas2D::drawLine(float x1, float y1, float x2, float y2) {
    if (!backend_) return;
    if (batching_) {
        queue_.drawLine(x1, y1, x2, y2);
        return;
    }
    backend_->setStrokeColor(strokeColor_);
    backend_->drawLine(x1, y1, x2, y2);
}

void Canvas2D::fillCircle(float x, float y, float radius, int segments) {
    if (!backend_ || segments < 3) return;
    if (batching_) {
        queue_.fillCircle(x, y, radius, segments);
        return;
    }
    backend_->setFillColor(fillColor_);
    backend_->fillCircle(x, y, radius, segments);
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
    if (!backend_ || path_.size() < 3) return;
    if (batching_) {
        queue_.fillPath(path_, pathClosed_);
        return;
    }
    backend_->fillPath(path_, pathClosed_, fillColor_);
}

void Canvas2D::stroke() {
    if (!backend_ || path_.size() < 2) return;
    if (batching_) {
        queue_.strokePath(path_, pathClosed_);
        return;
    }
    backend_->strokePath(path_, pathClosed_, strokeColor_, lineWidth_);
}

void Canvas2D::drawMaterial(float x, float y, const Material& material) {
    if (!backend_) return;
    void* handle = backend_->uploadMaterial(material);
    if (!handle) return;
    if (batching_) {
        queue_.drawMaterial(x, y, handle);
        return;
    }
    backend_->drawMaterial(x, y, handle);
}

void Canvas2D::drawTexture(float x, float y, RenderTexture* texture) {
    drawTexture(x, y, texture, BlendMode::Blend);
}

void Canvas2D::drawTexture(float x, float y, RenderTexture* texture, BlendMode mode) {
    if (!backend_) return;
    flush();
    backend_->drawTexture(x, y, texture, mode);
}

void Canvas2D::setRenderTarget(RenderTexture* target) {
    if (!backend_) return;
    if (target == currentTarget_) return;
    flush();
    backend_->setTarget(target);
    currentTarget_ = target;
}

void Canvas2D::clear(const Color& c) {
    if (!backend_) return;
    flush();
    backend_->clear(c);
}

void Canvas2D::setClipRect(float x, float y, float w, float h) {
    if (!backend_) return;
    flush();
    backend_->setClipRect(x, y, w, h);
}

void Canvas2D::resetClipRect() {
    if (!backend_) return;
    flush();
    backend_->resetClipRect();
}

void Canvas2D::begin3D() {
    if (!backend_ || in3D_) return;
    flush();
    if (backend_->lock3DTarget(&lockedPixels_, &lockedPitch_)) {
        in3D_ = true;
        std::fill(depthBuffer_.begin(), depthBuffer_.end(), 1.0f);
    }
}

void Canvas2D::end3D() {
    if (!in3D_) return;
    in3D_ = false;
    backend_->unlock3DTarget();
    backend_->present3DTarget();
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

    uint8_t cr = (uint8_t)(color.r * 255.0f);
    uint8_t cg = (uint8_t)(color.g * 255.0f);
    uint8_t cb = (uint8_t)(color.b * 255.0f);
    uint8_t ca = (uint8_t)(color.a * 255.0f);

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
                row[px + 0] = cr;
                row[px + 1] = cg;
                row[px + 2] = cb;
                row[px + 3] = ca;
            }
        }
    }
}

void Canvas2D::fillCube3D(float cx, float cy, float size, float rotX, float rotY,
                          float rotZ, const Color& color) {
    fillBox3D(cx, cy, size, size, size, rotX, rotY, rotZ, color);
}

void Canvas2D::fillBox3D(float cx, float cy, float sx, float sy, float sz,
                         float rotX, float rotY, float rotZ, const Color& color) {
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

    float cxr = std::cos(rotX), sxr = std::sin(rotX);
    float cyr = std::cos(rotY), syr = std::sin(rotY);
    float czr = std::cos(rotZ), szr = std::sin(rotZ);

    auto rotate = [&](const Vec3& p) -> Vec3 {
        Vec3 r1(p.x, p.y * cxr - p.z * sxr, p.y * sxr + p.z * cxr);
        Vec3 r2(r1.x * cyr + r1.z * syr, r1.y, -r1.x * syr + r1.z * cyr);
        return Vec3(r2.x * czr - r2.y * szr, r2.x * szr + r2.y * czr, r2.z);
    };

    const float cameraDist = 60.0f;
    const float projScale = 60.0f;
    auto project = [&](const Vec3& p) -> Vec2 {
        float denom = p.z + cameraDist;
        if (denom < 1.0f) denom = 1.0f;
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
        if (n.z > 0.0f) continue;

        float ny = n.y / n.length();
        float l = 0.65f + 0.35f * ny;
        if (l < 0.4f) l = 0.4f;
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

void Canvas2D::drawMesh3D(float cx, float cy, float scale,
                          float rotX, float rotY, float rotZ,
                          const Vec3* vertices, int vertexCount,
                          const int* indices, int triangleCount,
                          const Color& color) {
    if (!in3D_ || !vertices || vertexCount <= 0 || !indices || triangleCount <= 0) return;

    float cxr = std::cos(rotX), sxr = std::sin(rotX);
    float cyr = std::cos(rotY), syr = std::sin(rotY);
    float czr = std::cos(rotZ), szr = std::sin(rotZ);

    auto rotate = [&](const Vec3& p) -> Vec3 {
        Vec3 r1(p.x, p.y * cxr - p.z * sxr, p.y * sxr + p.z * cxr);
        Vec3 r2(r1.x * cyr + r1.z * syr, r1.y, -r1.x * syr + r1.z * cyr);
        return Vec3(r2.x * czr - r2.y * szr, r2.x * szr + r2.y * czr, r2.z);
    };

    const float cameraDist = 60.0f;
    const float projScale = 60.0f * scale;
    auto project = [&](const Vec3& p) -> Vec2 {
        float denom = p.z + cameraDist;
        if (denom < 1.0f) denom = 1.0f;
        return Vec2(cx + p.x / denom * projScale, cy - p.y / denom * projScale);
    };

    std::vector<Vec3> tv(vertexCount);
    for (int i = 0; i < vertexCount; ++i) tv[i] = rotate(vertices[i]);

    for (int t = 0; t < triangleCount; ++t) {
        int i0 = indices[t * 3 + 0];
        int i1 = indices[t * 3 + 1];
        int i2 = indices[t * 3 + 2];
        if (i0 < 0 || i0 >= vertexCount || i1 < 0 || i1 >= vertexCount || i2 < 0 || i2 >= vertexCount) continue;

        const Vec3& a = tv[i0];
        const Vec3& b = tv[i1];
        const Vec3& c_ = tv[i2];

        Vec3 n = Vec3::cross(b - a, c_ - a);
        if (n.z > 0.0f) continue;

        float ny = n.y / n.length();
        float l = 0.65f + 0.35f * ny;
        if (l < 0.4f) l = 0.4f;
        if (l > 1.0f) l = 1.0f;
        Color shaded(color.r * l, color.g * l, color.b * l, color.a);

        Vec2 p0 = project(a);
        Vec2 p1 = project(b);
        Vec2 p2 = project(c_);
        fillTriangle3D(p0, p1, p2, a.z, b.z, c_.z, shaded);
    }
}

} // namespace domi
