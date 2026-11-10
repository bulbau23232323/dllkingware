#pragma once

#include "vec.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kingware::game {

// A single player as observed through the game memory reader.
struct PlayerSnapshot {
    std::uint64_t entity = 0;
    std::string name;
    v3::Vec3 position{};  // feet
    v3::Vec3 head{};     // approximated head position
    v3::Vec3 velocity{};
    float health = 0.0f;
    float max_health = 100.0f;
    std::uint32_t team = 0;
    std::uint32_t flags = 0;
    bool alive = false;
    bool sleeping = false;
    bool wounded = false;
    bool is_local = false;
    bool is_npc = false;
    bool valid = false;
};

// Everything the renderer and the aim logic need for one frame.
struct WorldSnapshot {
    bool camera_valid = false;
    v3::Mat4 view_projection{};
    v3::Vec3 camera_position{};
    v3::MatrixLayout layout = v3::MatrixLayout::StrideFour;
    float field_of_view = 90.0f;
    std::vector<PlayerSnapshot> players;
    int entity_count = 0;
    std::string error;
};

// Classification of the local player for team filtering.
enum class TeamRelation { Unknown, Same, Different };

inline TeamRelation relation_to_local(const WorldSnapshot& world, std::uint32_t team) {
    for (const PlayerSnapshot& player : world.players) {
        if (!player.is_local) continue;
        if (player.team == team) return TeamRelation::Same;
        return TeamRelation::Different;
    }
    return TeamRelation::Unknown;
}

} // namespace kingware::game
