#include "domi/canvas2d.h"
#include "domi/backend/render_backend.h"
#include "domi/render_texture.h"
#include "domi/ui/font.h"
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

void Canvas2D::save() {
    stateStack_.push_back(state_);
}

void Canvas2D::restore() {
    if (stateStack_.empty()) return;
    state_ = stateStack_.back();
    stateStack_.pop_back();
}

void Canvas2D::translate(float x, float y) {
    state_.transform = state_.transform * Affine2D::translate(x, y);
}

void Canvas2D::rotate(float radians) {
    state_.transform = state_.transform * Affine2D::rotate(radians);
}

void Canvas2D::scale(float x, float y) {
    state_.transform = state_.transform * Affine2D::scale(x, y);
}

void Canvas2D::transform(float a, float b, float c, float d, float e, float f) {
    state_.transform = state_.transform * Affine2D(a, b, c, d, e, f);
}

void Canvas2D::setTransform(float a, float b, float c, float d, float e, float f) {
    state_.transform = Affine2D(a, b, c, d, e, f);
}

void Canvas2D::resetTransform() {
    state_.transform.identity();
}

void Canvas2D::setFillColor(const Color& c) {
    state_.fillColor = c;
    if (batching_) {
        queue_.setFillColor(c);
    }
}

void Canvas2D::setStrokeColor(const Color& c) {
    state_.strokeColor = c;
    if (batching_) {
        queue_.setStrokeColor(c);
    }
}

void Canvas2D::setLineWidth(float w) {
    state_.lineWidth = w;
    if (batching_) {
        queue_.setLineWidth(w);
    }
}

void Canvas2D::applyTransform(std::vector<Vec2>& points) const {
    for (size_t i = 0; i < points.size(); ++i) {
        points[i] = state_.transform * points[i];
    }
}

void Canvas2D::fillRect(float x, float y, float w, float h) {
    if (!backend_) return;
    std::vector<Vec2> rect;
    rect.reserve(4);
    rect.push_back(Vec2(x, y));
    rect.push_back(Vec2(x + w, y));
    rect.push_back(Vec2(x + w, y + h));
    rect.push_back(Vec2(x, y + h));
    applyTransform(rect);
    if (batching_) {
        queue_.fillPath(rect, true);
        return;
    }
    backend_->setFillColor(state_.fillColor);
    backend_->fillPath(rect, true, state_.fillColor);
}

void Canvas2D::strokeRect(float x, float y, float w, float h) {
    if (!backend_) return;
    std::vector<Vec2> rect;
    rect.reserve(4);
    rect.push_back(Vec2(x, y));
    rect.push_back(Vec2(x + w, y));
    rect.push_back(Vec2(x + w, y + h));
    rect.push_back(Vec2(x, y + h));
    applyTransform(rect);
    if (batching_) {
        queue_.strokePath(rect, true);
        return;
    }
    backend_->setStrokeColor(state_.strokeColor);
    backend_->strokePath(rect, true, state_.strokeColor, state_.lineWidth);
}

void Canvas2D::clearRect(float x, float y, float w, float h) {
    if (batching_) {
        std::vector<Vec2> rect;
        rect.reserve(4);
        rect.push_back(Vec2(x, y));
        rect.push_back(Vec2(x + w, y));
        rect.push_back(Vec2(x + w, y + h));
        rect.push_back(Vec2(x, y + h));
        applyTransform(rect);
        queue_.fillPath(rect, true);
    } else {
        fillRect(x, y, w, h);
    }
}

void Canvas2D::drawLine(float x1, float y1, float x2, float y2) {
    if (!backend_) return;
    Vec2 a = applyTransform(x1, y1);
    Vec2 b = applyTransform(x2, y2);
    if (batching_) {
        queue_.drawLine(a.x, a.y, b.x, b.y);
        return;
    }
    backend_->setStrokeColor(state_.strokeColor);
    backend_->drawLine(a.x, a.y, b.x, b.y);
}

void Canvas2D::fillCircle(float x, float y, float radius, int segments) {
    if (!backend_ || segments < 3) return;
    std::vector<Vec2> circle;
    circle.reserve(segments + 1);
    for (int i = 0; i < segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * 3.14159265f;
        circle.push_back(Vec2(x + radius * std::cos(angle),
                              y + radius * std::sin(angle)));
    }
    applyTransform(circle);
    if (batching_) {
        queue_.fillPath(circle, true);
        return;
    }
    backend_->setFillColor(state_.fillColor);
    backend_->fillPath(circle, true, state_.fillColor);
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
    std::vector<Vec2> transformed = path_;
    applyTransform(transformed);
    if (batching_) {
        queue_.fillPath(transformed, pathClosed_);
        return;
    }
    backend_->fillPath(transformed, pathClosed_, state_.fillColor);
}

void Canvas2D::stroke() {
    if (!backend_ || path_.size() < 2) return;
    std::vector<Vec2> transformed = path_;
    applyTransform(transformed);
    if (batching_) {
        queue_.strokePath(transformed, pathClosed_);
        return;
    }
    backend_->setStrokeColor(state_.strokeColor);
    backend_->strokePath(transformed, pathClosed_, state_.strokeColor, state_.lineWidth);
}

namespace {

const float PI = 3.14159265f;

float normalizeAngle(float a) {
    while (a < -PI) a += 2.0f * PI;
    while (a >  PI) a -= 2.0f * PI;
    return a;
}

} // anonymous namespace

void Canvas2D::arc(float x, float y, float radius,
                   float startAngle, float endAngle,
                   bool counterClockwise) {
    if (radius < 0.0f) return;
    float delta = endAngle - startAngle;
    if (!counterClockwise && delta < 0.0f) delta += 2.0f * PI;
    if (counterClockwise && delta > 0.0f) delta -= 2.0f * PI;
    if (!counterClockwise && delta > 2.0f * PI) delta = 2.0f * PI;
    if (counterClockwise && delta < -2.0f * PI) delta = -2.0f * PI;

    float sx = x + radius * std::cos(startAngle);
    float sy = y + radius * std::sin(startAngle);
    if (path_.empty()) {
        path_.push_back(Vec2(sx, sy));
    } else {
        // Standard canvas: line from current point to arc start.
        path_.push_back(Vec2(sx, sy));
    }

    int segments = std::max(8, static_cast<int>(std::abs(delta) * 30.0f));
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        float angle = startAngle + delta * t;
        path_.push_back(Vec2(x + radius * std::cos(angle),
                             y + radius * std::sin(angle)));
    }
}

void Canvas2D::arcTo(float x1, float y1, float x2, float y2, float radius) {
    if (radius < 0.0f || path_.empty()) return;

    Vec2 p0 = path_.back();
    Vec2 p1(x1, y1);
    Vec2 p2(x2, y2);

    Vec2 v1 = p0 - p1;
    Vec2 v2 = p2 - p1;
    float len1 = v1.length();
    float len2 = v2.length();
    if (len1 < 0.0001f || len2 < 0.0001f) {
        path_.push_back(p1);
        return;
    }

    Vec2 u1 = v1 * (1.0f / len1);
    Vec2 u2 = v2 * (1.0f / len2);
    float cross = u1.x * u2.y - u1.y * u2.x;
    float dot = u1.x * u2.x + u1.y * u2.y;
    float angle = std::atan2(std::abs(cross), dot);
    if (angle < 0.001f || std::abs(angle - PI) < 0.001f) {
        path_.push_back(p1);
        return;
    }

    float d = radius / std::tan(angle * 0.5f);
    if (d > len1 || d > len2) {
        path_.push_back(p1);
        return;
    }

    Vec2 t1 = p1 + u1 * d;

    // Center at p1 + (u1 + u2) * radius / sin(angle).
    Vec2 bisector = u1 + u2;
    if (bisector.length() < 0.0001f) {
        path_.push_back(p1);
        return;
    }
    // Center lies on the internal angle bisector at distance r / sin(angle)
    // from the corner p1 (equivalent to r/sin(angle/2) along the unit bisector).
    Vec2 center = p1 + bisector * (radius / std::sin(angle));

    // The path turns according to the sign of cross; the arc around the center
    // turns the opposite way because the center lies on the inside of the turn.
    bool arcCCW = cross < 0.0f;
    float startA = std::atan2(t1.y - center.y, t1.x - center.x);

    // Line from current point p0 to tangent point t1, then arc to t2.
    path_.push_back(t1);

    // The arc spans the same angle as the path turn, but in the opposite
    // rotational direction because it curves around the inside center.
    float delta = arcCCW ? angle : -angle;
    int segments = std::max(8, static_cast<int>(std::abs(delta) * 30.0f));
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        float a = startA + delta * t;
        path_.push_back(Vec2(center.x + radius * std::cos(a),
                             center.y + radius * std::sin(a)));
    }
}

void Canvas2D::bezierCurveTo(float cp1x, float cp1y,
                             float cp2x, float cp2y,
                             float x, float y) {
    if (path_.empty()) {
        path_.push_back(Vec2(cp1x, cp1y));
    }
    Vec2 p0 = path_.back();
    Vec2 p1(cp1x, cp1y);
    Vec2 p2(cp2x, cp2y);
    Vec2 p3(x, y);

    const int segments = 24;
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        float t2 = t * t;
        float px = mt2 * mt * p0.x + 3.0f * mt2 * t * p1.x +
                   3.0f * mt * t2 * p2.x + t2 * t * p3.x;
        float py = mt2 * mt * p0.y + 3.0f * mt2 * t * p1.y +
                   3.0f * mt * t2 * p2.y + t2 * t * p3.y;
        path_.push_back(Vec2(px, py));
    }
}

void Canvas2D::quadraticCurveTo(float cpx, float cpy, float x, float y) {
    if (path_.empty()) {
        path_.push_back(Vec2(cpx, cpy));
    }
    Vec2 p0 = path_.back();
    Vec2 p1(cpx, cpy);
    Vec2 p2(x, y);

    const int segments = 24;
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        float mt = 1.0f - t;
        float px = mt * mt * p0.x + 2.0f * mt * t * p1.x + t * t * p2.x;
        float py = mt * mt * p0.y + 2.0f * mt * t * p1.y + t * t * p2.y;
        path_.push_back(Vec2(px, py));
    }
}

void Canvas2D::ellipse(float x, float y, float radiusX, float radiusY,
                       float rotation, float startAngle, float endAngle,
                       bool counterClockwise) {
    if (radiusX < 0.0f || radiusY < 0.0f) return;
    float delta = endAngle - startAngle;
    if (!counterClockwise && delta < 0.0f) delta += 2.0f * PI;
    if (counterClockwise && delta > 0.0f) delta -= 2.0f * PI;
    if (!counterClockwise && delta > 2.0f * PI) delta = 2.0f * PI;
    if (counterClockwise && delta < -2.0f * PI) delta = -2.0f * PI;

    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);

    auto point = [&](float angle) -> Vec2 {
        float lx = radiusX * std::cos(angle);
        float ly = radiusY * std::sin(angle);
        return Vec2(x + lx * cosR - ly * sinR,
                    y + lx * sinR + ly * cosR);
    };

    Vec2 startP = point(startAngle);
    if (path_.empty()) {
        path_.push_back(startP);
    } else {
        path_.push_back(startP);
    }

    int segments = std::max(8, static_cast<int>(std::abs(delta) * 30.0f));
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        float angle = startAngle + delta * t;
        path_.push_back(point(angle));
    }
}

void Canvas2D::drawMaterial(float x, float y, const Material& material) {
    if (!backend_) return;
    void* handle = backend_->uploadMaterial(material);
    if (!handle) return;

    float tx, ty, angle, sx, sy;
    state_.transform.decompose(&tx, &ty, &angle, &sx, &sy);
    Vec2 p = applyTransform(x, y);
    float centerX = tx - p.x;
    float centerY = ty - p.y;

    if (batching_) {
        queue_.drawMaterial(p.x, p.y, handle, angle, centerX, centerY, sx, sy);
        return;
    }
    backend_->drawMaterial(p.x, p.y, handle, angle, centerX, centerY, sx, sy);
}

void Canvas2D::drawText(float x, float y, const char* text, Font* font, const Color& color) {
    if (!font) return;
    font->drawText(this, x, y, text, color);
}

void Canvas2D::drawTexture(float x, float y, RenderTexture* texture) {
    drawTexture(x, y, texture, BlendMode::Blend);
}

void Canvas2D::drawTexture(float x, float y, RenderTexture* texture, BlendMode mode) {
    if (!backend_ || !texture) return;
    flush();

    float tx, ty, angle, sx, sy;
    state_.transform.decompose(&tx, &ty, &angle, &sx, &sy);
    Vec2 p = applyTransform(x, y);
    float centerX = tx - p.x;
    float centerY = ty - p.y;

    backend_->drawTexture(p.x, p.y, texture, mode, angle, centerX, centerY, sx, sy);
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

    Vec2 p0 = applyTransform(x, y);
    Vec2 p1 = applyTransform(x + w, y);
    Vec2 p2 = applyTransform(x + w, y + h);
    Vec2 p3 = applyTransform(x, y + h);

    float minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
    float minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
    float maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
    float maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));

    backend_->setClipRect(minX, minY, maxX - minX, maxY - minY);
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
