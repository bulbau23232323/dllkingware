#pragma once

#include <optional>
#include <vector>

namespace kingware::aim {

// Coordinates are projected 2D positions supplied by a data source. This
// module neither reads game memory nor sends mouse / view-angle input.
struct Vec2 { float x = 0.0f, y = 0.0f; };
enum class Bone { Head = 0, Neck = 1, Chest = 2, Nearest = 3 };

struct Hitbox {
    Bone bone = Bone::Head;
    Vec2 center{};
    float radius = 0.0f;
    bool visible = false;
};

struct Target {
    int id = 0;
    bool alive = true;
    bool teammate = false;
    float distance = 0.0f;
    std::vector<Hitbox> hitboxes;
};

struct Options {
    bool enabled = false;
    bool visible_only = true;
    bool ignore_team = true;
    bool multipoint = true;
    Bone bone = Bone::Head;
    float fov_radius = 80.0f;  // Projected screen units, not world degrees.
    float max_distance = 1000.0f;
    float point_scale = 0.65f; // [0, 1] fraction of hitbox radius.
    float smooth = 8.0f;       // 1 = immediate; higher = slower.
    float humanization = 0.0f; // [0, 1] bounded preview-only wobble.
};

struct Solution {
    int target_id = 0;
    Bone bone = Bone::Head;
    Vec2 point{};
    Vec2 delta{}; // Proposed movement for this frame, not applied to the OS.
    float point_distance = 0.0f;
};

// Selects the closest valid projected point and computes a frame-rate-aware
// movement step. Missing/invalid data is rejected rather than guessed.
std::optional<Solution> solve(const Vec2& cursor, const std::vector<Target>& targets,
                              const Options& options, float delta_seconds,
                              float elapsed_seconds);

} // namespace kingware::aim
