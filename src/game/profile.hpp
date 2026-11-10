#pragma once

#include <cstdint>

namespace kingware::game {

// ---------------------------------------------------------------------------
// Offset profiles
//
// A profile describes one concrete game build: which modules to open, where
// the static/class pointers live and where the player fields sit.
//
// Two profiles are shipped:
//
//   * "Rust Devblog 240" - the offset list that the project owner supplied.
//     It is kept verbatim because it could not be verified against a running
//     client. It has no camera and no health offsets, so the reader reports
//     those values as unavailable instead of inventing them.
//
//   * "Rust il2cpp" - the build that the owner's own reference release
//     documents (build 25129251). Every number comes from that release's
//     offset header; the traversal code in this project is written from
//     scratch and does not share any code with it.
//
// A profile is data only. The reader in reader.hpp is shared by both.
// ---------------------------------------------------------------------------

// A pointer chain that starts at a module base.
//
//   value = read_pointer(module_base + module_offset)
//   for every step: value = read_pointer(value + step)
//
// The Devblog-240 list writes its global entries as "0x2FF92B0 + 0x12F +
// 0xE0"; that form is modelled by module_offset + two steps. The exact
// dereference semantics of that list are not documented, therefore the reader
// prints every resolved link in the diagnostics page so a wrong step is
// visible instead of silently producing garbage.
struct StaticChain {
    const char* name = "";
    std::uintptr_t module_offset = 0;
    std::uintptr_t steps[3] = {0, 0, 0};
    int step_count = 0;
};

// Field offsets of one player class. A zero offset means "this profile does
// not contain the field"; the reader then reports the value as unavailable.
struct PlayerFields {
    std::uintptr_t health = 0;
    std::uintptr_t max_health = 0;
    std::uintptr_t life_state = 0;
    std::uintptr_t flags = 0;
    std::uintptr_t team = 0;
    std::uintptr_t display_name = 0;
    std::uintptr_t model = 0;
    std::uintptr_t player_input = 0;
    std::uintptr_t eyes = 0;
    std::uintptr_t inventory = 0;
    std::uintptr_t active_item = 0;
};

// Complete description of one game build.
struct GameProfile {
    const char* name = "";
    const char* build = "";
    const char* client_module = "";
    const char* code_module = "";  // module that holds the static offsets
    const char* unity_module = "";

    // --- entity list ---
    // "encrypted" style: class pointer -> static fields -> encrypted handle.
    bool encrypted_statics = false;
    std::uintptr_t networkable_type_info = 0;
    std::uintptr_t static_fields = 0;
    std::uintptr_t wrapper_class = 0;
    std::uintptr_t parent_statics = 0;
    std::uintptr_t entities = 0;
    std::uintptr_t handle_value = 0;  // offset of the encrypted handle field
    StaticChain entity_chain{};        // plain pointer chain style

    // --- camera ---
    std::uintptr_t camera_type_info = 0;
    std::uintptr_t camera_object = 0;
    std::uintptr_t view_matrix_native = 0;
    std::uintptr_t camera_position = 0;
    std::uintptr_t camera_fov = 0;
    StaticChain camera_chain{};

    // --- players ---
    PlayerFields player{};
    std::uintptr_t player_model_position = 0;
    std::uintptr_t player_model_velocity = 0;
    std::uintptr_t entity_class = 0;      // Il2CppObject::klass
    std::uintptr_t class_name = 0;         // Il2CppClass::name
    std::uintptr_t prefab_id = 0;          // entity prefab id (fast player test)
    std::uintptr_t prefab_name = 0;        // GameObject prefab name
    std::uintptr_t game_object = 0;
    std::uintptr_t game_object_tag = 0;

    // --- shared il2cpp object layout ---
    std::uintptr_t native_cached_ptr = 0;
    std::uintptr_t string_length = 0;
    std::uintptr_t string_chars = 0;
    std::uintptr_t list_items = 0;
    std::uintptr_t list_count = 0;
    std::uintptr_t array_first = 0;
    std::uintptr_t array_length = 0;

    bool has_camera() const { return camera_type_info != 0 || camera_chain.step_count > 0; }
    bool has_health() const { return player.health != 0; }
    bool has_team() const { return player.team != 0; }
};

// The list that the project owner supplied for this task. It is the public
// "Rust Devblog 240" dump and is kept exactly as delivered.
inline constexpr GameProfile kDevblog240Profile{
    /*name=*/"Rust Devblog 240",
    /*build=*/"devblog 240 (owner supplied)",
    /*client_module=*/"RustClient.exe",
    /*code_module=*/"GameAssembly.dll",
    /*unity_module=*/"UnityPlayer.dll",
    /*encrypted_statics=*/false,
    /*networkable_type_info=*/0,
    /*static_fields=*/0,
    /*wrapper_class=*/0,
    /*parent_statics=*/0,
    /*entities=*/0,
    /*handle_value=*/0,
    StaticChain{"BaseNetworkable", 0x2FF92B0, {0x12F, 0xE0, 0}, 2},
    /*camera_type_info=*/0,
    /*camera_object=*/0,
    /*view_matrix_native=*/0,
    /*camera_position=*/0,
    /*camera_fov=*/0,
    StaticChain{"MainCamera", 0x2FFA3F8, {0x12F, 0xE0, 0}, 2},
    PlayerFields{/*health=*/0, /*max_health=*/0, /*life_state=*/0, /*flags=*/0x5F8,
                 /*team=*/0x538, /*display_name=*/0, /*model=*/0x4A8, /*player_input=*/0x4C8,
                 /*eyes=*/0x600, /*inventory=*/0x608, /*active_item=*/0x570},
    /*player_model_position=*/0,
    /*player_model_velocity=*/0,
    /*entity_class=*/0,
    /*class_name=*/0,
    /*prefab_id=*/0,
    /*prefab_name=*/0,
    /*game_object=*/0,
    /*game_object_tag=*/0,
    /*native_cached_ptr=*/0,
    /*string_length=*/0,
    /*string_chars=*/0,
    /*list_items=*/0,
    /*list_count=*/0,
    /*array_first=*/0,
    /*array_length=*/0,
};

// Build 25129251, the build documented by the owner's reference release.
inline constexpr GameProfile kIl2Cpp25129251Profile{
    /*name=*/"Rust il2cpp",
    /*build=*/"build 25129251",
    /*client_module=*/"RustClient.exe",
    /*code_module=*/"GameAssembly.dll",
    /*unity_module=*/"UnityPlayer.dll",
    /*encrypted_statics=*/true,
    /*networkable_type_info=*/0x119536D8,
    /*static_fields=*/0xB8,
    /*wrapper_class=*/0x8,
    /*parent_statics=*/0x10,
    /*entities=*/0x20,
    /*handle_value=*/0x18,
    StaticChain{"BaseNetworkable", 0, {0, 0, 0}, 0},
    /*camera_type_info=*/0x1191DB58,
    /*camera_object=*/0x28,
    /*view_matrix_native=*/0x2FC,
    /*camera_position=*/0x444,
    /*camera_fov=*/0x170,
    StaticChain{"MainCamera", 0, {0, 0, 0}, 0},
    PlayerFields{/*health=*/0x2B4, /*max_health=*/0x2B8, /*life_state=*/0x2A8, /*flags=*/0x6D8,
                 /*team=*/0x558, /*display_name=*/0x300, /*model=*/0x5B8, /*player_input=*/0x518,
                 /*eyes=*/0x7A0, /*inventory=*/0x7A8, /*active_item=*/0x588},
    /*player_model_position=*/0x2F8,
    /*player_model_velocity=*/0x31C,
    /*entity_class=*/0x0,
    /*class_name=*/0x10,
    /*prefab_id=*/0x54,
    /*prefab_name=*/0x60,
    /*game_object=*/0x20,
    /*game_object_tag=*/0x44,
    /*native_cached_ptr=*/0x10,
    /*string_length=*/0x10,
    /*string_chars=*/0x14,
    /*list_items=*/0x10,
    /*list_count=*/0x18,
    /*array_first=*/0x20,
    /*array_length=*/0x18,
};

inline constexpr GameProfile* const kGameProfiles[] = {
    &kDevblog240Profile,
    &kIl2Cpp25129251Profile,
};
inline constexpr int kGameProfileCount = 2;

// PlayerFlags bits used for the state colouring.
inline constexpr std::uint32_t kFlagSleeping = 4;
inline constexpr std::uint32_t kFlagWounded = 16;

// LifeState values: 0 alive, 1 dead.
inline constexpr std::uint32_t kLifeStateAlive = 0;

} // namespace kingware::game
