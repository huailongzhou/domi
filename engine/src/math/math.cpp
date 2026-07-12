#include "domi/math.h"
#include <cmath>

namespace domi {

Quat Quat::fromEuler(float pitch, float yaw, float roll) {
    float cy = std::cos(yaw * 0.5f);
    float sy = std::sin(yaw * 0.5f);
    float cp = std::cos(pitch * 0.5f);
    float sp = std::sin(pitch * 0.5f);
    float cr = std::cos(roll * 0.5f);
    float sr = std::sin(roll * 0.5f);
    return Quat(
        sr * cp * cy - cr * sp * sy,
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy
    );
}

Vec3 Quat::toEuler() const {
    Vec3 euler;
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    euler.x = std::atan2(sinr_cosp, cosr_cosp);
    float sinp = 2.0f * (w * y - z * x);
    if (std::abs(sinp) >= 1)
        euler.y = std::copysign(3.14159265f / 2.0f, sinp);
    else
        euler.y = std::asin(sinp);
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    euler.z = std::atan2(siny_cosp, cosy_cosp);
    return euler;
}

Mat4 Mat4::translate(const Vec3& t) {
    Mat4 m;
    m.m[12] = t.x; m.m[13] = t.y; m.m[14] = t.z;
    return m;
}

Mat4 Mat4::scale(const Vec3& s) {
    Mat4 m;
    m.m[0] = s.x; m.m[5] = s.y; m.m[10] = s.z;
    return m;
}

Mat4 Mat4::rotate(const Quat& q) {
    Mat4 m;
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    m.m[0] = 1 - 2 * (yy + zz); m.m[4] = 2 * (xy - wz);     m.m[8]  = 2 * (xz + wy);
    m.m[1] = 2 * (xy + wz);     m.m[5] = 1 - 2 * (xx + zz); m.m[9]  = 2 * (yz - wx);
    m.m[2] = 2 * (xz - wy);     m.m[6] = 2 * (yz + wx);     m.m[10] = 1 - 2 * (xx + yy);
    return m;
}

Mat4 Mat4::perspective(float fov, float aspect, float near, float far) {
    Mat4 m;
    float f = 1.0f / std::tan(fov * 0.5f);
    float nf = 1.0f / (near - far);
    m.m[0] = f / aspect; m.m[5] = f;
    m.m[10] = (far + near) * nf; m.m[11] = -1.0f;
    m.m[14] = 2.0f * far * near * nf; m.m[15] = 0;
    return m;
}

Mat4 Mat4::ortho(float left, float right, float bottom, float top, float near, float far) {
    Mat4 m;
    m.m[0] = 2.0f / (right - left);
    m.m[5] = 2.0f / (top - bottom);
    m.m[10] = -2.0f / (far - near);
    m.m[12] = -(right + left) / (right - left);
    m.m[13] = -(top + bottom) / (top - bottom);
    m.m[14] = -(far + near) / (far - near);
    return m;
}

Mat4 Mat4::lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 f = (target - eye).normalized();
    Vec3 s = Vec3::cross(f, up).normalized();
    Vec3 u = Vec3::cross(s, f);
    Mat4 m;
    m.m[0] = s.x; m.m[4] = s.y; m.m[8]  = s.z;
    m.m[1] = u.x; m.m[5] = u.y; m.m[9]  = u.z;
    m.m[2] = -f.x; m.m[6] = -f.y; m.m[10] = -f.z;
    m.m[12] = -Vec3::dot(s, eye);
    m.m[13] = -Vec3::dot(u, eye);
    m.m[14] = Vec3::dot(f, eye);
    return m;
}

Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i * 4 + j] = m[i * 4 + 0] * o.m[0 * 4 + j] +
                             m[i * 4 + 1] * o.m[1 * 4 + j] +
                             m[i * 4 + 2] * o.m[2 * 4 + j] +
                             m[i * 4 + 3] * o.m[3 * 4 + j];
    return r;
}

Vec4 Mat4::operator*(const Vec4& v) const {
    return Vec4(
        m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12] * v.w,
        m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13] * v.w,
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
        m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
    );
}

Vec3 Transform::forward() const {
    Vec3 f;
    f.x = 2 * (rotation.x * rotation.z + rotation.w * rotation.y);
    f.y = 2 * (rotation.y * rotation.z - rotation.w * rotation.x);
    f.z = 1 - 2 * (rotation.x * rotation.x + rotation.y * rotation.y);
    return f.normalized();
}

Vec3 Transform::right() const {
    Vec3 r;
    r.x = 1 - 2 * (rotation.y * rotation.y + rotation.z * rotation.z);
    r.y = 2 * (rotation.x * rotation.y + rotation.w * rotation.z);
    r.z = 2 * (rotation.x * rotation.z - rotation.w * rotation.y);
    return r.normalized();
}

Vec3 Transform::up() const {
    Vec3 u;
    u.x = 2 * (rotation.x * rotation.y - rotation.w * rotation.z);
    u.y = 1 - 2 * (rotation.x * rotation.x + rotation.z * rotation.z);
    u.z = 2 * (rotation.y * rotation.z + rotation.w * rotation.x);
    return u.normalized();
}

} // namespace domi
