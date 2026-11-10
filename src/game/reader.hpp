#pragma once

#include "memory_reader.hpp"
#include "profile.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kingware::game {

// Base addresses of the modules the reader needs. They are resolved by the
// platform layer (driver module-base query or the tool-help snapshot) and
// handed to the reader, which keeps the traversal itself platform neutral.
struct ModuleSet {
    std::uint64_t client = 0;  // RustClient.exe
    std::uint64_t code = 0;    // GameAssembly.dll
    std::uint64_t unity = 0;   // UnityPlayer.dll
};

// Everything the reader managed to resolve in the last update. Fields that a
// profile does not describe stay at their default value and are reported as
// unavailable instead of being filled with a guess.
struct ReadReport {
    bool camera_ok = false;
    bool entity_list_ok = false;
    bool players_ok = false;
    int entity_count = 0;
    int player_count = 0;
    std::uint64_t camera_class = 0;
    std::uint64_t camera_native = 0;
    std::uint64_t networkable_class = 0;
    std::uint64_t realm = 0;
    std::uint64_t collection = 0;
    std::uint64_t buffer = 0;
    std::string error;
};

// Reads the world state of one Rust client through a memory source.
//
// Two profile styles are supported:
//   * "encrypted" - the il2cpp build where the static fields of
//     BaseNetworkable hold encrypted handles that have to be decoded and
//     resolved through the GC handle table.
//   * "chain" - the plain pointer chain style used by the Devblog-240 list.
class GameReader {
public:
    GameReader(const MemoryReader& memory, const GameProfile& profile, ModuleSet modules)
        : memory_(memory), profile_(profile), modules_(modules) {}

    const GameProfile& profile() const { return profile_; }
    ModuleSet& modules() { return modules_; }

    // Resolves the camera and the entity collection, then reads the players.
    bool update(WorldSnapshot& world, ReadReport& report);

    // Resolves only the camera; used by the fast render path.
    bool update_camera(WorldSnapshot& world, ReadReport& report);

private:
    template <typename T>
    bool read_value(std::uint64_t address, T& out) const {
        static_assert(std::is_trivially_copyable<T>::value, "read_value needs a plain type");
        return memory_.read(address, &out, sizeof(T));
    }

    template <typename T>
    T read_or(std::uint64_t address, T fallback = T{}) const {
        T value{};
        if (!read_value(address, value)) return fallback;
        return value;
    }

    bool resolve_entity_collection(std::uint64_t& buffer, int& count, ReadReport& report);
    bool resolve_realm(std::uint64_t& realm, ReadReport& report);
    bool resolve_gun_handle(std::uint64_t& handle, ReadReport& report);
    bool read_player(std::uint64_t entity, PlayerSnapshot& player);
    bool is_player(std::uint64_t entity) const;
    bool read_managed_string(std::uint64_t address, std::string& out) const;

    const MemoryReader& memory_;
    const GameProfile& profile_;
    ModuleSet modules_;
};

// Converts a UTF-16 buffer into UTF-8. Exposed for the tests.
std::string utf16_to_utf8(const std::uint16_t* text, std::size_t length);

} // namespace kingware::game
