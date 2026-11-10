#include "aim.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace kingware::aim {
namespace {

bool finite(Vec2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
float length_squared(Vec2 p) { return p.x * p.x + p.y * p.y; }

} // namespace

std::optional<Solution> solve(const Vec2& cursor, const std::vector<Target>& targets,
                              const Options& options, float delta_seconds,
                              float elapsed_seconds) {
    if (!options.enabled || !finite(cursor) || !std::isfinite(options.fov_radius) ||
        options.fov_radius <= 0.0f || !std::isfinite(options.max_distance) ||
        options.max_distance < 0.0f || !std::isfinite(options.point_scale) ||
        !std::isfinite(options.smooth) || !std::isfinite(options.humanization) ||
        !std::isfinite(delta_seconds) || delta_seconds <= 0.0f ||
        !std::isfinite(elapsed_seconds)) return std::nullopt;

    const float max_sq = options.fov_radius * options.fov_radius;
    if (!std::isfinite(max_sq)) return std::nullopt;
    float best_sq = std::numeric_limits<float>::infinity();
    std::optional<Solution> best;
    const float point_scale = std::clamp(options.point_scale, 0.0f, 1.0f);

    for (const Target& target : targets) {
        if (!target.alive || (options.ignore_team && target.teammate) ||
            !std::isfinite(target.distance) || target.distance < 0.0f || target.distance > options.max_distance)
            continue;
        for (const Hitbox& hitbox : target.hitboxes) {
            if ((options.bone != Bone::Nearest && hitbox.bone != options.bone) ||
                (options.visible_only && !hitbox.visible) || !finite(hitbox.center) ||
                !std::isfinite(hitbox.radius) || hitbox.radius < 0.0f)
                continue;

            const float radius = hitbox.radius * point_scale;
            // These are geometric sample points only. A future backend must
            // provide trustworthy hitbox visibility before they are usable.
            const Vec2 points[] = {
                hitbox.center,
                {hitbox.center.x - radius, hitbox.center.y},
                {hitbox.center.x + radius, hitbox.center.y},
                {hitbox.center.x, hitbox.center.y - radius},
                {hitbox.center.x, hitbox.center.y + radius},
            };
            const int count = options.multipoint ? 5 : 1;
            for (int i = 0; i < count; ++i) {
                const Vec2 diff{points[i].x - cursor.x, points[i].y - cursor.y};
                const float sq = length_squared(diff);
                if (!std::isfinite(sq) || sq > max_sq || sq >= best_sq) continue;
                best_sq = sq;
                best = Solution{target.id, hitbox.bone, points[i], {}, std::sqrt(sq)};
            }
        }
    }
    if (!best) return std::nullopt;

    const Vec2 diff{best->point.x - cursor.x, best->point.y - cursor.y};
    const float smooth = std::max(1.0f, options.smooth);
    const float dt = std::clamp(delta_seconds, 0.0f, 0.25f);
    const float alpha = smooth <= 1.0f ? 1.0f : 1.0f - std::pow(1.0f - 1.0f / smooth, dt * 60.0f);
    best->delta = {diff.x * alpha, diff.y * alpha};

    if (best->point_distance > 0.001f && options.humanization > 0.0f && alpha < 1.0f) {
        // Subtle, bounded perpendicular motion for the visual simulation.
        // It is not an anti-detection or anti-cheat feature.
        const float amount = std::clamp(options.humanization, 0.0f, 1.0f);
        const float phase = static_cast<float>(std::sin(static_cast<double>(elapsed_seconds) * 3.7 +
                                                        static_cast<double>(best->target_id)));
        const float wobble = phase * std::min(1.0f, best->point_distance * 0.1f) * amount * alpha;
        best->delta.x += (-diff.y / best->point_distance) * wobble;
        best->delta.y += (diff.x / best->point_distance) * wobble;
    }
    const float movement = std::sqrt(length_squared(best->delta));
    if (movement > best->point_distance && movement > 0.0f) {
        const float scale = best->point_distance / movement;
        best->delta.x *= scale;
        best->delta.y *= scale;
    }
    return best;
}

} // namespace kingware::aim
