#pragma once

#include <cmath>
#include <cstdint>

namespace kingware::v3 {

// Small 3D math helper used by the world/screen bridge. The unit tests verify
// the projection against an analytic perspective matrix, so a wrong convention
// is caught without a running game client.
struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

// Unity stores its matrices with the translation in the last row and
// multiplies row vectors (v * M). The sixteen floats below are read in that
// order.
struct Mat4 {
    float m[16]{};

    static Mat4 identity() {
        Mat4 result{};
        result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
        return result;
    }

    static Mat4 from_raw(const float* raw) {
        Mat4 result{};
        for (int i = 0; i < 16; ++i) result.m[i] = raw[i];
        return result;
    }
};

// How the sixteen floats of a native matrix have to be interpreted.
//
// Unity's managed Matrix4x4 stores its elements column by column, so the
// row-vector formula needs a stride of four. Some native structures keep the
// same matrix in a plain row-major block, where the formula needs a stride of
// one. Both readings are implemented and the reader picks the one that is
// consistent with the scene.
enum class MatrixLayout {
    StrideFour = 0, // Unity's column-major storage, row-vector maths
    StrideOne = 1,  // row-major storage, row-vector maths
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

struct ScreenPoint {
    float x = 0.0f, y = 0.0f;
    bool visible = false; // false when the point is behind the camera
};

// Projects a world position with Unity's row-vector convention:
// clip.x = x*m00 + y*m10 + z*m20 + m30, and so on for the other rows.
inline ScreenPoint project(const Mat4& view_projection, const Vec3& world,
                            float viewport_width, float viewport_height,
                            MatrixLayout layout = MatrixLayout::StrideFour) {
    ScreenPoint result;
    if (!valid(world) || viewport_width <= 0.0f || viewport_height <= 0.0f) return result;
    const int stride = layout == MatrixLayout::StrideFour ? 4 : 1;
    const float x = world.x, y = world.y, z = world.z;
    const float clip_x = x * view_projection.m[0] + y * view_projection.m[stride] +
                         z * view_projection.m[2 * stride] + view_projection.m[3 * stride];
    const float clip_y = x * view_projection.m[1] + y * view_projection.m[1 + stride] +
                         z * view_projection.m[1 + 2 * stride] + view_projection.m[1 + 3 * stride];
    const float clip_w = x * view_projection.m[3] + y * view_projection.m[3 + stride] +
                         z * view_projection.m[3 + 2 * stride] + view_projection.m[3 + 3 * stride];
    if (!std::isfinite(clip_w) || clip_w <= 0.0001f) return result;
    const float inverse_w = 1.0f / clip_w;
    const float ndc_x = clip_x * inverse_w;
    const float ndc_y = clip_y * inverse_w;
    if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y)) return result;
    result.x = (ndc_x * 0.5f + 0.5f) * viewport_width;
    result.y = (1.0f - (ndc_y * 0.5f + 0.5f)) * viewport_height;
    result.visible = true;
    return result;
}

// The world-space forward direction of the camera, derived from the matrix
// itself. Used to pick the correct matrix layout at runtime.
inline Vec3 forward_axis(const Mat4& matrix, MatrixLayout layout) {
    const int stride = layout == MatrixLayout::StrideFour ? 4 : 1;
    return {matrix.m[2 * stride], matrix.m[1 + 2 * stride], matrix.m[2 + 2 * stride]};
}

// Builds an analytic perspective projection in Unity's native storage order
// (column-major, row-vector maths). Used by the tests as an independent
// reference for the projection above.
inline Mat4 perspective(float fov_radians, float aspect, float near_plane, float far_plane) {
    Mat4 result{};
    const float focal = 1.0f / std::tan(fov_radians * 0.5f);
    // Column-major storage: element (row, column) lives at column * 4 + row.
    result.m[0] = focal / aspect;                                        // m00
    result.m[5] = focal;                                                // m11
    result.m[10] = (far_plane + near_plane) / (near_plane - far_plane); // m22
    result.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane); // m32
    result.m[11] = -1.0f;                                               // m23
    result.m[15] = 1.0f;                                                // m33
    return result;
}

// Converts a world-space direction into horizontal/vertical degrees.
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
