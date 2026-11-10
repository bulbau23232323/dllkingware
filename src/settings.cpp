#include "settings.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <locale>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace kingware {
namespace {

std::string trim(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

bool parse_bool(const std::string& text, bool& value) {
    if (text == "1" || text == "true") { value = true; return true; }
    if (text == "0" || text == "false") { value = false; return true; }
    return false;
}

template<typename T>
bool parse_number(const std::string& text, T& value, T low, T high) {
    std::istringstream stream(text);
    stream.imbue(std::locale::classic());
    T parsed{};
    stream >> parsed;
    if (stream.fail()) return false;
    stream >> std::ws;
    if (!stream.eof() || !std::isfinite(static_cast<double>(parsed))) return false;
    value = std::clamp(parsed, low, high);
    return true;
}

bool parse_color(const std::string& text, float (&color)[4]) {
    if (std::count(text.begin(), text.end(), ',') != 3) return false;
    std::istringstream input(text);
    std::string part;
    float values[4]{};
    for (int i = 0; i < 4; ++i) {
        if (!std::getline(input, part, ',') || !parse_number(trim(part), values[i], 0.0f, 1.0f))
            return false;
    }
    if (std::getline(input, part, ',')) return false;
    std::copy(std::begin(values), std::end(values), std::begin(color));
    return true;
}

std::string color_string(const float (&color)[4]) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << color[0] << ',' << color[1] << ',' << color[2] << ',' << color[3];
    return out.str();
}

} // namespace

std::string serialize_settings(const Settings& s) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9);
    out << "; Kingware GUI profile. Values are not a game connection.\n"
        << "[aim]\n"
        << "enabled=" << s.aim_enabled << '\n'
        << "visible_only=" << s.aim_visible_only << '\n'
        << "ignore_team=" << s.aim_ignore_team << '\n'
        << "multipoint=" << s.aim_multipoint << '\n'
        << "draw_fov=" << s.aim_draw_fov << '\n'
        << "bone=" << s.aim_bone << '\n'
        << "key=" << s.aim_key << '\n'
        << "fov=" << s.aim_fov << '\n'
        << "smooth=" << s.aim_smooth << '\n'
        << "humanization=" << s.aim_humanization << '\n'
        << "point_scale=" << s.aim_point_scale << '\n'
        << "\n[visuals]\n"
        << "enabled=" << s.visual_enabled << '\n'
        << "box=" << s.visual_box << '\n'
        << "health=" << s.visual_health << '\n'
        << "armor=" << s.visual_armor << '\n'
        << "name=" << s.visual_name << '\n'
        << "weapon=" << s.visual_weapon << '\n'
        << "distance=" << s.visual_distance << '\n'
        << "skeleton=" << s.visual_skeleton << '\n'
        << "max_distance=" << s.visual_distance_max << '\n'
        << "chams=" << s.visual_chams << '\n'
        << "visible_color=" << color_string(s.visual_color) << '\n'
        << "hidden_color=" << color_string(s.visual_hidden_color) << '\n'
        << "\n[resolver]\n"
        << "enabled=" << s.resolver_enabled << '\n'
        << "prediction=" << s.resolver_prediction << '\n'
        << "interpolation=" << s.resolver_interpolation << '\n'
        << "mode=" << s.resolver_mode << '\n'
        << "strength=" << s.resolver_strength << '\n'
        << "max_time=" << s.resolver_max_time << '\n'
        << "\n[misc]\n"
        << "watermark=" << s.misc_watermark << '\n'
        << "fps=" << s.misc_fps << '\n'
        << "crosshair=" << s.misc_crosshair << '\n';
    return out.str();
}

Settings parse_settings(const std::string& text, ParseReport* report) {
    Settings s;
    ParseReport stats;
    std::istringstream input(text);
    std::string section, line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        auto equals = line.find('=');
        if (equals == std::string::npos) { ++stats.invalid; continue; }
        const auto key = section + '.' + trim(line.substr(0, equals));
        const auto val = trim(line.substr(equals + 1));
        bool known = true, ok = false;
#define BOOL_ENTRY(name, field) if (key == name) ok = parse_bool(val, s.field)
#define INT_ENTRY(name, field, lo, hi) if (key == name) ok = parse_number(val, s.field, lo, hi)
#define FLOAT_ENTRY(name, field, lo, hi) if (key == name) ok = parse_number(val, s.field, lo, hi)
        BOOL_ENTRY("aim.enabled", aim_enabled);
        else BOOL_ENTRY("aim.visible_only", aim_visible_only);
        else BOOL_ENTRY("aim.ignore_team", aim_ignore_team);
        else BOOL_ENTRY("aim.multipoint", aim_multipoint);
        else BOOL_ENTRY("aim.draw_fov", aim_draw_fov);
        else INT_ENTRY("aim.bone", aim_bone, 0, 3);
        else INT_ENTRY("aim.key", aim_key, 0, 3);
        else FLOAT_ENTRY("aim.fov", aim_fov, 1.0f, 180.0f);
        else FLOAT_ENTRY("aim.smooth", aim_smooth, 1.0f, 30.0f);
        else FLOAT_ENTRY("aim.humanization", aim_humanization, 0.0f, 100.0f);
        else FLOAT_ENTRY("aim.point_scale", aim_point_scale, 0.0f, 100.0f);
        else BOOL_ENTRY("visuals.enabled", visual_enabled);
        else INT_ENTRY("visuals.box", visual_box, 0, 2);
        else BOOL_ENTRY("visuals.health", visual_health);
        else BOOL_ENTRY("visuals.armor", visual_armor);
        else BOOL_ENTRY("visuals.name", visual_name);
        else BOOL_ENTRY("visuals.weapon", visual_weapon);
        else BOOL_ENTRY("visuals.distance", visual_distance);
        else BOOL_ENTRY("visuals.skeleton", visual_skeleton);
        else FLOAT_ENTRY("visuals.max_distance", visual_distance_max, 50.0f, 1000.0f);
        else INT_ENTRY("visuals.chams", visual_chams, 0, 3);
        else if (key == "visuals.visible_color") ok = parse_color(val, s.visual_color);
        else if (key == "visuals.hidden_color") ok = parse_color(val, s.visual_hidden_color);
        else BOOL_ENTRY("resolver.enabled", resolver_enabled);
        else BOOL_ENTRY("resolver.prediction", resolver_prediction);
        else BOOL_ENTRY("resolver.interpolation", resolver_interpolation);
        else INT_ENTRY("resolver.mode", resolver_mode, 0, 2);
        else FLOAT_ENTRY("resolver.strength", resolver_strength, 0.0f, 100.0f);
        else FLOAT_ENTRY("resolver.max_time", resolver_max_time, 0.0f, 300.0f);
        else BOOL_ENTRY("misc.watermark", misc_watermark);
        else BOOL_ENTRY("misc.fps", misc_fps);
        else BOOL_ENTRY("misc.crosshair", misc_crosshair);
        else known = false;
#undef BOOL_ENTRY
#undef INT_ENTRY
#undef FLOAT_ENTRY
        if (known) {
            ++stats.recognized;
            if (!ok) ++stats.invalid;
        }
    }
    if (report) *report = stats;
    return s;
}

LoadResult load_settings(const std::filesystem::path& path, Settings& out, std::string& error) {
    error.clear();
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) { error = ec.message(); return LoadResult::Error; }
    if (!exists) return LoadResult::NotFound;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 64 * 1024) {
        error = ec ? ec.message() : "Profile is larger than 64 KiB";
        return LoadResult::Error;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "Could not open profile for reading"; return LoadResult::Error; }
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad()) { error = "Could not read profile"; return LoadResult::Error; }
    ParseReport report;
    out = parse_settings(contents, &report);
    if (report.invalid > 0) {
        error = "Some profile values were invalid; defaults were used";
        return LoadResult::LoadedWithWarnings;
    }
    return LoadResult::Loaded;
}

bool save_settings(const std::filesystem::path& path, const Settings& settings, std::string& error) {
    error.clear();
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) { error = ec.message(); return false; }
    }
    // Independent temporary names keep two running instances from writing the
    // same file concurrently. The final rename replaces the profile atomically.
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto process_id = static_cast<unsigned long long>(GetCurrentProcessId());
#else
    const auto process_id = static_cast<unsigned long long>(getpid());
#endif
    auto temporary = path;
    temporary += ".tmp." + std::to_string(process_id) + "." + std::to_string(sequence.fetch_add(1));
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) { error = "Could not open temporary profile for writing"; return false; }
        file << serialize_settings(settings);
        file.flush();
        file.close();
        if (!file) { error = "Could not write profile"; std::filesystem::remove(temporary, ec); return false; }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Could not replace profile (Windows error " + std::to_string(GetLastError()) + ")";
        std::filesystem::remove(temporary, ec);
        return false;
    }
#else
    std::filesystem::rename(temporary, path, ec);
    if (ec) { error = ec.message(); std::filesystem::remove(temporary, ec); return false; }
#endif
    return true;
}

std::filesystem::path default_settings_path() {
#ifdef _WIN32
    wchar_t value[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", value, static_cast<DWORD>(std::size(value)));
    if (length > 0 && length < std::size(value)) return std::filesystem::path(value) / L"Kingware" / L"settings.ini";
    // A writable working directory is a last resort if APPDATA is unavailable.
    return std::filesystem::path(L"settings.ini");
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        if (*xdg) return std::filesystem::path(xdg) / "kingware" / "settings.ini";
    if (const char* home = std::getenv("HOME"))
        if (*home) return std::filesystem::path(home) / ".config" / "kingware" / "settings.ini";
    return std::filesystem::path("settings.ini");
#endif
}

} // namespace kingware
