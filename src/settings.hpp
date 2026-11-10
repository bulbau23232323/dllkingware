#pragma once

#include <filesystem>
#include <string>

namespace kingware {

// These are configuration values only. Until a real backend is connected,
// enabling an option changes the demo, not the game.
struct Settings {
    bool aim_enabled = false;
    bool aim_visible_only = true;
    bool aim_ignore_team = true;
    bool aim_multipoint = true;
    bool aim_draw_fov = true;
    int aim_bone = 0;       // Head, neck, chest, nearest
    int aim_key = 0;        // RMB, Mouse 4, LShift, LAlt
    float aim_fov = 80.0f;
    float aim_smooth = 8.0f;
    float aim_humanization = 15.0f;
    float aim_point_scale = 65.0f;

    bool visual_enabled = true;
    int visual_box = 1;     // Off, corners, full
    bool visual_health = true;
    bool visual_armor = true;
    bool visual_name = true;
    bool visual_weapon = true;
    bool visual_distance = true;
    bool visual_skeleton = false;
    float visual_distance_max = 250.0f;
    int visual_chams = 0;   // Off, flat, outline, wireframe
    float visual_color[4] = {1.0f, 0.43f, 0.17f, 1.0f};
    float visual_hidden_color[4] = {0.58f, 0.62f, 0.65f, 1.0f};

    bool resolver_enabled = false;
    bool resolver_prediction = true;
    bool resolver_interpolation = true;
    int resolver_mode = 0;   // Adaptive, velocity, interpolation
    float resolver_strength = 65.0f;
    float resolver_max_time = 120.0f;

    bool misc_watermark = true;
    bool misc_fps = false;
    bool misc_crosshair = false;
};

struct ParseReport {
    int recognized = 0;
    int invalid = 0;
};

enum class LoadResult { Loaded, LoadedWithWarnings, NotFound, Error };

std::string serialize_settings(const Settings& settings);
// Parses known keys, validates ranges and keeps defaults for malformed values.
Settings parse_settings(const std::string& text, ParseReport* report = nullptr);
LoadResult load_settings(const std::filesystem::path& path, Settings& out, std::string& error);
bool save_settings(const std::filesystem::path& path, const Settings& settings, std::string& error);
std::filesystem::path default_settings_path();

} // namespace kingware
