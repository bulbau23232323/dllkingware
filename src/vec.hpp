#pragma once

#include <cmath>
#include <cstdint>

namespace kingware::v3 {

// Minimal 3D math used by the world/screen bridge. The unit tests verify the
// projection against an analytic perspective matrix, so a wrong convention is
// caught without a running game.
struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

// Column-major 4x4 matrix, matching the OpenGL/Unity projection convention
// used by the game render pipeline.
struct Mat4 {
    float m[16]{};

    static Mat4 identity() {
        Mat4 result{};
        result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
        return result;
    }
};

inline Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 scale(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline float distance(const Vec3& a, const Vec3& b) { return length(sub(a, b)); }
inline bool valid(const Vec3& a) {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}

// Multiplies a column-major matrix by a column vector.
inline Vec4 transform(const Mat4& matrix, const Vec3& point) {
    Vec4 result;
    result.x = matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12];
    result.y = matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13];
    result.z = matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14];
    result.w = matrix.m[3] * point.x + matrix.m[7] * point.y + matrix.m[11] * point.z + matrix.m[15];
    return result;
}

struct ScreenPoint {
    float x = 0.0f, y = 0.0f;
    bool visible = false; // false when the point is behind the camera
};

// Projects a world position into screen pixels. The viewport is the full
// client area of the overlay window.
inline ScreenPoint world_to_screen(const Mat4& view_projection, const Vec3& world,
                                   float viewport_width, float viewport_height) {
    ScreenPoint result;
    if (!valid(world) || viewport_width <= 0.0f || viewport_height <= 0.0f) return result;
    const Vec4 clip = transform(view_projection, world);
    if (!std::isfinite(clip.w) || clip.w <= 0.0001f) return result;
    const float inverse_w = 1.0f / clip.w;
    const float ndc_x = clip.x * inverse_w;
    const float ndc_y = clip.y * inverse_w;
    if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y)) return result;
    result.x = (ndc_x * 0.5f + 0.5f) * viewport_width;
    result.y = (1.0f - (ndc_y * 0.5f + 0.5f)) * viewport_height;
    result.visible = true;
    return result;
}

// Builds a right-handed perspective projection matrix with the Unity
// convention (depth in [-1, 1]). Used by the tests as an analytic reference.
inline Mat4 perspective(float fov_radians, float aspect, float near_plane, float far_plane) {
    Mat4 result{};
    const float focal = 1.0f / std::tan(fov_radians * 0.5f);
    result.m[0] = focal / aspect;
    result.m[5] = focal;
    result.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

// Converts a world-space direction into the horizontal/vertical angle that the
// game camera would need to look at that point. Used by the prediction code.
inline Vec2 direction_to_angle(const Vec3& from, const Vec3& to) {
    const Vec3 delta = sub(to, from);
    if (!valid(delta)) return {0.0f, 0.0f};
    const float horizontal = std::atan2(delta.x, delta.z) * 57.29577951308232f;
    const float vertical = std::atan2(delta.y, std::sqrt(delta.x * delta.x + delta.z * delta.z)) *
                           57.29577951308232f;
    return {horizontal, vertical};
}

inline float normalize_angle(float degrees) {
    while (degrees > 180.0f) degrees -= 360.0f;
    while (degrees < -180.0f) degrees += 360.0f;
    return degrees;
}

} // namespace kingware::v3
