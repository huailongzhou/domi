#ifndef DOMI_MATH_H
#define DOMI_MATH_H

#include <cmath>
#include <cstdint>

namespace domi {

struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    float length() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const { float l = length(); return l > 0 ? Vec2(x / l, y / l) : Vec2(); }
};

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const { float l = length(); return l > 0 ? Vec3(x / l, y / l, z / l) : Vec3(); }
    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }
    static float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
};

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

struct Quat {
    float x, y, z, w;
    Quat() : x(0), y(0), z(0), w(1) {}
    Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    static Quat fromEuler(float pitch, float yaw, float roll);
    Vec3 toEuler() const;
};

struct Mat4 {
    float m[16];
    Mat4() { identity(); }
    void identity() {
        for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
    static Mat4 translate(const Vec3& t);
    static Mat4 scale(const Vec3& s);
    static Mat4 rotate(const Quat& q);
    static Mat4 perspective(float fov, float aspect, float near, float far);
    static Mat4 ortho(float left, float right, float bottom, float top, float near, float far);
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up);
    Mat4 operator*(const Mat4& o) const;
    Vec4 operator*(const Vec4& v) const;
};

struct Color {
    float r, g, b, a;
    Color() : r(1), g(1), b(1), a(1) {}
    Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const Color& o) const { return !(*this == o); }
    uint32_t toRGBA() const {
        return ((uint32_t)(r * 255) << 24) | ((uint32_t)(g * 255) << 16) |
               ((uint32_t)(b * 255) << 8) | (uint32_t)(a * 255);
    }
};

struct Rect {
    float x, y, w, h;
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// 2D affine transform used by Canvas2D.
// Matrix layout matches the HTML canvas transform(a,b,c,d,e,f):
//   x' = m[0]*x + m[2]*y + m[4]
//   y' = m[1]*x + m[3]*y + m[5]
struct Affine2D {
    float m[6];
    Affine2D() { identity(); }
    explicit Affine2D(float a, float b, float c, float d, float e, float f) {
        m[0] = a; m[1] = b; m[2] = c; m[3] = d; m[4] = e; m[5] = f;
    }
    void identity() { m[0] = 1; m[1] = 0; m[2] = 0; m[3] = 1; m[4] = 0; m[5] = 0; }
    static Affine2D translate(float x, float y) { return Affine2D(1, 0, 0, 1, x, y); }
    static Affine2D rotate(float radians) {
        float c = std::cos(radians);
        float s = std::sin(radians);
        return Affine2D(c, s, -s, c, 0, 0);
    }
    static Affine2D scale(float x, float y) { return Affine2D(x, 0, 0, y, 0, 0); }
    Affine2D operator*(const Affine2D& o) const {
        Affine2D r;
        r.m[0] = m[0]*o.m[0] + m[2]*o.m[1];
        r.m[1] = m[1]*o.m[0] + m[3]*o.m[1];
        r.m[2] = m[0]*o.m[2] + m[2]*o.m[3];
        r.m[3] = m[1]*o.m[2] + m[3]*o.m[3];
        r.m[4] = m[0]*o.m[4] + m[2]*o.m[5] + m[4];
        r.m[5] = m[1]*o.m[4] + m[3]*o.m[5] + m[5];
        return r;
    }
    Vec2 operator*(const Vec2& v) const {
        return Vec2(m[0]*v.x + m[2]*v.y + m[4],
                    m[1]*v.x + m[3]*v.y + m[5]);
    }
    // Approximate decomposition into translation, rotation and scale.
    // Returns rotation in radians. Ignores shear.
    void decompose(float* tx, float* ty, float* rotation,
                   float* sx, float* sy) const {
        if (tx) *tx = m[4];
        if (ty) *ty = m[5];
        if (sx) *sx = std::sqrt(m[0]*m[0] + m[1]*m[1]);
        if (sy) *sy = std::sqrt(m[2]*m[2] + m[3]*m[3]);
        if (rotation) *rotation = std::atan2(m[1], m[0]);
    }
};

struct AABB {
    Vec3 min, max;
    AABB() {}
    AABB(const Vec3& min_, const Vec3& max_) : min(min_), max(max_) {}
    bool contains(const Vec3& p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
    }
    bool intersects(const AABB& o) const {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }
};

struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
    Transform() : position(), rotation(), scale(1, 1, 1) {}
    Mat4 toMatrix() const {
        return Mat4::translate(position) * Mat4::rotate(rotation) * Mat4::scale(scale);
    }
    Vec3 forward() const;
    Vec3 right() const;
    Vec3 up() const;
};

} // namespace domi

#endif
