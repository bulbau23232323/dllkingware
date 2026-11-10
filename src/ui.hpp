#pragma once

#include "aim.hpp"
#include "settings.hpp"
#include "memory_reader.hpp"
#include <imgui.h>
#include <string>

namespace kingware {

enum class Page { Aimbot, Visuals, Resolver, Misc, Debug };
enum class ProfileStatus { Defaults, Loaded, Saving, Saved, Error };

struct AppState {
    Settings settings;
    Page page = Page::Visuals;
    ProfileStatus profile_status = ProfileStatus::Defaults;
    DriverState driver_state = DriverState::NotImplemented;
    std::string profile_error;
    bool request_close = false;
    bool request_minimize = false;
    bool request_save = false;
    bool demo_visible = true;
    char offset_filter[96]{};
};

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* small_font = nullptr;
    ImFont* tiny = nullptr;
    ImFont* bold = nullptr;
    ImFont* heading = nullptr;
    ImFont* brand = nullptr;
    ImFont* mono = nullptr;
};

// Cross-platform ImGui UI; the actual window/backend is implemented separately.
class Interface {
public:
    // Call after ImGui::CreateContext, and again after a DPI change before NewFrame.
    void configure(float dpi_scale = 1.0f);
    void render(AppState& state);

private:
    float scale_ = 1.0f;
    Fonts fonts_{};
    aim::Vec2 preview_cursor_{};
    Page previous_page_ = Page::Visuals;
};

} // namespace kingware
