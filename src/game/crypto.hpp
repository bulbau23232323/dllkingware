#pragma once

#include <cstdint>

namespace kingware::game {

// Value transformations that the current client applies to a few managed
// pointers before they become usable.
//
// The transformations were derived from the offset table and helper headers
// that the project owner supplied for this task. The code below is an
// independent implementation: the numeric constants and the order of the
// operations are facts about the game build, the implementation is our own.
//
// Every helper works on a 64-bit word whose two 32-bit halves are processed
// independently, which is how the client encodes these values.
namespace crypto {

inline std::uint32_t rotate_left(std::uint32_t value, unsigned bits) {
    bits &= 31u;
    if (bits == 0) return value;
    return (value << bits) | (value >> (32u - bits));
}

inline std::uint32_t rotate_right(std::uint32_t value, unsigned bits) {
    bits &= 31u;
    if (bits == 0) return value;
    return (value >> bits) | (value << (32u - bits));
}

// Applies a per-half transformation to both halves of a 64-bit word.
template <typename Half>
inline std::uint64_t map_halves(std::uint64_t value, Half transform) {
    std::uint32_t low = static_cast<std::uint32_t>(value & 0xFFFFFFFFull);
    std::uint32_t high = static_cast<std::uint32_t>(value >> 32);
    low = transform(low);
    high = transform(high);
    return static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
}

// Decrypts the wrapper that guards the entity realm handle.
inline std::uint64_t decode_realm(std::uint64_t value) {
    return map_halves(value, [](std::uint32_t part) {
        part += 0x7A62B45Du;
        part ^= 0xDDF0990Bu;
        return rotate_left(part, 27);
    });
}

// Decrypts the entity collection that is stored inside the realm.
inline std::uint64_t decode_entity_collection(std::uint64_t value) {
    return map_halves(value, [](std::uint32_t part) {
        part ^= 0x18EE70EFu;
        part = rotate_left(part, 1);
        part ^= 0xDF94DFA3u;
        part += 0x2A6262F8u;
        return part;
    });
}

// Decrypts the PlayerEyes pointer of a player.
inline std::uint64_t decode_player_eyes(std::uint64_t value) {
    return map_halves(value, [](std::uint32_t part) {
        part ^= 0x073DF469u;
        part -= 0x061E7F65u;
        part ^= 0x35841624u;
        return part;
    });
}

// Decrypts the active item pointer of a player.
inline std::uint64_t decode_active_item(std::uint64_t value) {
    return map_halves(value, [](std::uint32_t part) {
        part ^= 0x537C2A57u;
        part += 0xE00025B3u;
        return rotate_left(part, 18);
    });
}

// Decrypts the inventory pointer of a player.
inline std::uint64_t decode_inventory(std::uint64_t value) {
    return map_halves(value, [](std::uint32_t part) {
        part += 0xF4BDEEB6u;
        part = rotate_left(part, 27);
        part += 0x4056D625u;
        part = rotate_left(part, 28);
        return part;
    });
}

// Decrypts the field of view value of the camera.
inline std::uint32_t decode_fov(std::uint32_t value) {
    value += 0x0DF0A59u;
    value ^= 0x22016506u;
    value = rotate_left(value, 22);
    value -= 0x136C7CC5u;
    return value;
}

} // namespace crypto

} // namespace kingware::game
