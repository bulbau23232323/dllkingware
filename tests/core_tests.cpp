#include "aim.hpp"
#include "offsets.hpp"
#include "memory_reader.hpp"
#include "settings.hpp"

#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
bool near(float a, float b, float epsilon = 0.001f) { return std::fabs(a - b) < epsilon; }
}

int main() {
    using namespace kingware;
    {
        Settings original;
        original.aim_enabled = true;
        original.aim_fov = 107.5f;
        original.aim_smooth = 12.25f;
        original.visual_box = 2;
        original.visual_color[0] = 0.125f;
        original.visual_color[1] = 0.75f;
        original.resolver_mode = 2;
        original.misc_fps = true;
        ParseReport report;
        const std::string serialized = serialize_settings(original);
        const auto parsed = parse_settings(serialized, &report);
        check(report.invalid == 0 && report.recognized >= 30, "all profile fields parse");
        check(serialize_settings(parsed) == serialized, "profile round trip is lossless");
    }
    {
        ParseReport report;
        const auto parsed = parse_settings(
            "[aim]\n"
            "enabled=not-a-bool\n"
            "fov=9999\n"
            "smooth=nan\n"
            "key=-20\n"
            "[visuals]\n"
            "box=99\n"
            "visible_color=1,0.5,0.2,1,\n"
            "[unknown]\nnew_field=whatever\n", &report);
        check(report.invalid == 3, "invalid fields are reported");
        check(!parsed.aim_enabled && near(parsed.aim_smooth, 8.0f), "bad values retain defaults");
        check(near(parsed.aim_fov, 180.0f) && parsed.aim_key == 0 && parsed.visual_box == 2, "numeric bounds are clamped");
        check(near(parsed.visual_color[1], 0.43f), "invalid color is not partially applied");
    }
    {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto dir = std::filesystem::temp_directory_path() / ("kingware_cpp_test_" + std::to_string(unique));
        std::error_code ec;
        const bool created = std::filesystem::create_directory(dir, ec);
        check(created && !ec, "isolated test directory created");
        if (!created) return 1; // Never remove a pre-existing directory.
        const auto path = dir / "nested" / "settings.ini";
        Settings saved;
        saved.visual_name = false;
        std::string error;
        check(save_settings(path, saved, error), "profile can be saved in a new directory");
        saved.aim_smooth = 4.0f;
        check(save_settings(path, saved, error), "existing profile can be replaced atomically");
        Settings loaded;
        check(load_settings(path, loaded, error) == LoadResult::Loaded, "saved profile loads");
        check(!loaded.visual_name && near(loaded.aim_smooth, 4.0f), "saved values are restored");
        check(load_settings(dir / "missing.ini", loaded, error) == LoadResult::NotFound, "missing profile handled");
        {
            std::ofstream huge(dir / "huge.ini", std::ios::binary);
            huge << std::string(65537, 'x');
        }
        check(load_settings(dir / "huge.ini", loaded, error) == LoadResult::Error, "oversized profile rejected");
        {
            std::ofstream invalid(dir / "invalid.ini");
            invalid << "[aim]\nenabled=not-a-bool\n";
        }
        check(load_settings(dir / "invalid.ini", loaded, error) == LoadResult::LoadedWithWarnings,
              "malformed profile is reported");
        {
            std::ofstream blocker(dir / "blocker");
            blocker << "not a directory";
        }
        check(!save_settings(dir / "blocker" / "settings.ini", saved, error) && !error.empty(),
              "save failure is reported");
        std::filesystem::remove_all(dir, ec);
    }
    {
        using namespace kingware::aim;
        const Target hidden{1, true, false, 30.0f, {{Bone::Head, {5, 0}, 2, false}}};
        const Target teammate{2, true, true, 30.0f, {{Bone::Head, {6, 0}, 2, true}}};
        const Target enemy{3, true, false, 30.0f, {{Bone::Head, {20, 0}, 4, true}, {Bone::Chest, {10, 0}, 2, true}}};
        Options o;
        o.enabled = true;
        o.multipoint = false;
        o.smooth = 1;
        auto result = solve({0,0}, {hidden, teammate, enemy}, o, 1.0f/60.0f, 0);
        check(result && result->target_id == 3 && near(result->point.x, 20), "visibility and team filters");
        o.bone = Bone::Nearest;
        result = solve({0,0}, {enemy}, o, 1.0f/60.0f, 0);
        check(result && result->bone == Bone::Chest, "nearest bone selection");
        o.bone = Bone::Head;
        o.multipoint = true;
        o.point_scale = 0.5f;
        result = solve({0,0}, {enemy}, o, 1.0f/60.0f, 0);
        check(result && near(result->point.x, 18.0f), "multipoint chooses closest geometric point");
        o.fov_radius = 10;
        check(!solve({0,0}, {enemy}, o, 1.0f/60.0f, 0), "points outside FOV rejected");
        o.fov_radius = 100;
        o.max_distance = 20;
        check(!solve({0,0}, {enemy}, o, 1.0f/60.0f, 0), "distance filter");
        o.max_distance = 100;
        o.visible_only = false;
        result = solve({0,0}, {hidden}, o, 1.0f/60.0f, 0);
        check(result && result->target_id == 1, "visibility filter can be disabled");
        o.enabled = false;
        check(!solve({0,0}, {enemy}, o, 1.0f/60.0f, 0), "disabled solver does nothing");
        o.enabled = true;
        check(!solve({std::numeric_limits<float>::quiet_NaN(),0}, {enemy}, o, 1.0f/60.0f, 0), "non-finite input rejected");
        check(!solve({0,0}, {enemy}, o, 0.0f, 0), "zero frame time rejected");
        o.smooth = 1.01f;
        o.humanization = 1.0f;
        result = solve({19,0}, {enemy}, o, 1.0f/60.0f, std::numeric_limits<float>::max());
        check(result && std::isfinite(result->delta.x) && std::isfinite(result->delta.y) &&
              std::hypot(result->delta.x, result->delta.y) <= result->point_distance + 0.0001f,
              "humanization stays finite and bounded");
    }
    {
        using namespace kingware::aim;
        const Target enemy{1, true, false, 10, {{Bone::Head, {100, 0}, 0, true}}};
        Options o;
        o.enabled = true; o.smooth = 10; o.fov_radius = 150; o.multipoint = false;
        Vec2 a{}, b{};
        for (int i = 0; i < 60; ++i) {
            auto result = solve(a, {enemy}, o, 1.0f / 60.0f, i / 60.0f);
            if (result) { a.x += result->delta.x; a.y += result->delta.y; }
        }
        for (int i = 0; i < 30; ++i) {
            auto result = solve(b, {enemy}, o, 1.0f / 30.0f, i / 30.0f);
            if (result) { b.x += result->delta.x; b.y += result->delta.y; }
        }
        check(near(a.x, b.x, .002f), "smoothing is frame-rate independent");
        check(a.x > 99.0f && a.x <= 100.0f, "smoothing approaches without overshooting");
    }
    {
        UnavailableMemoryReader driver;
        unsigned char destination[4] = {1, 2, 3, 4};
        check(driver.status() == DriverState::NotImplemented, "driver status reports no implementation, not a fake scan");
        check(!driver.read(0x1234, destination, sizeof(destination)), "stub never claims to read memory");
        check(destination[0] == 1 && destination[3] == 4, "stub does not alter the output buffer");
    }
    check(offset_count == 98, "all supplied offset entries included");
    if (failures) { std::cerr << failures << " test(s) failed\n"; return 1; }
    std::cout << "All core tests passed.\n";
    return 0;
}
