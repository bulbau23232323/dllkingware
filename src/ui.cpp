#include "ui.hpp"
#include "offsets.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace kingware {
namespace {

constexpr ImU32 BG = IM_COL32(15, 16, 17, 255);
constexpr ImU32 SIDEBAR = IM_COL32(19, 20, 21, 255);
constexpr ImU32 PANEL = IM_COL32(24, 25, 26, 255);
constexpr ImU32 BORDER = IM_COL32(47, 48, 49, 255);
constexpr ImU32 BORDER_SUBTLE = IM_COL32(37, 38, 39, 255);
constexpr ImU32 WHITE = IM_COL32(231, 231, 228, 255);
constexpr ImU32 MUTED = IM_COL32(137, 139, 138, 255);
constexpr ImU32 DIM = IM_COL32(91, 94, 94, 255);
constexpr ImU32 ORANGE = IM_COL32(255, 119, 55, 255);
constexpr ImU32 ORANGE_LIGHT = IM_COL32(255, 151, 94, 255);
constexpr ImU32 ORANGE_DARK = IM_COL32(60, 37, 26, 255);
constexpr ImU32 GREEN = IM_COL32(146, 195, 117, 255);
constexpr ImU32 BLUE = IM_COL32(112, 169, 204, 255);

ImVec2 add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
ImVec2 add(ImVec2 a, float x, float y) { return ImVec2(a.x + x, a.y + y); }
ImU32 with_alpha(ImU32 color, int alpha) { return (color & 0x00ffffffu) | (static_cast<ImU32>(alpha) << 24); }

bool file_exists(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return file.good();
}

ImFont* load_font(const std::vector<std::string>& paths, float size, const ImWchar* ranges) {
    for (const auto& path : paths) {
        if (file_exists(path)) {
            ImFontConfig config;
            config.OversampleH = 2;
            config.OversampleV = 2;
            if (ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size, &config, ranges)) return font;
        }
    }
    ImFontConfig config;
    config.SizePixels = size;
    return ImGui::GetIO().Fonts->AddFontDefault(&config);
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

const char* page_name(Page page) {
    switch (page) {
    case Page::Aimbot: return "AIMBOT";
    case Page::Visuals: return "VISUALS";
    case Page::Resolver: return "RESOLVER";
    case Page::Misc: return "MISCELLANEOUS";
    case Page::Debug: return "DEBUG";
    }
    return "VISUALS";
}

const char* driver_label(DriverState status) {
    switch (status) {
    case DriverState::NotImplemented: return "НЕТ АДАПТЕРА";
    case DriverState::Disconnected: return "НЕТ СВЯЗИ";
    case DriverState::Connected: return "ПОДКЛЮЧЁН";
    case DriverState::Error: return "ОШИБКА";
    }
    return "НЕИЗВЕСТНО";
}

const char* driver_detail(DriverState status) {
    return status == DriverState::NotImplemented ? "ИНТЕРФЕЙС НЕ РЕАЛИЗОВАН" : driver_label(status);
}

const char* profile_label(ProfileStatus status) {
    switch (status) {
    case ProfileStatus::Defaults: return "ПО УМОЛЧАНИЮ";
    case ProfileStatus::Loaded: return "ЗАГРУЖЕН";
    case ProfileStatus::Saving: return "СОХРАНЕНИЕ";
    case ProfileStatus::Saved: return "СОХРАНЁН";
    case ProfileStatus::Error: return "ОШИБКА";
    }
    return "—";
}

struct Renderer {
    float scale;
    Fonts& f;
    AppState& state;
    aim::Vec2& preview_cursor;
    Page& previous_page;

    float S(float value) const { return value * scale; }
    ImVec2 V(float x, float y) const { return ImVec2(S(x), S(y)); }
    float text_width(ImFont* font, const char* value) const {
        return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, value).x;
    }
    void text(ImDrawList* d, ImFont* font, ImVec2 p, ImU32 color, const char* value) const {
        d->AddText(font, font->FontSize, p, color, value);
    }
    void centered(ImDrawList* d, ImFont* font, float x, float y, ImU32 color, const char* value) const {
        text(d, font, ImVec2(x - text_width(font, value) * 0.5f, y), color, value);
    }
    void right(ImDrawList* d, ImFont* font, float x, float y, ImU32 color, const char* value) const {
        text(d, font, ImVec2(x - text_width(font, value), y), color, value);
    }
    void line(ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 col = BORDER_SUBTLE, float thickness = 1.0f) const {
        d->AddLine(a, b, col, S(thickness));
    }
    void dot(ImDrawList* d, ImVec2 p, ImU32 c, float radius = 3.0f) const {
        d->AddCircleFilled(p, S(radius), c, 16);
    }
    void card(ImVec2 p, float w, float h) const {
        auto* d = ImGui::GetWindowDrawList();
        d->AddRectFilled(p, add(p, w, S(h)), PANEL, S(6));
        d->AddRect(p, add(p, w, S(h)), BORDER, S(6), 0, S(1));
    }
    void card_header(ImVec2 p, float w, const char* title, const char* badge = nullptr) const {
        auto* d = ImGui::GetWindowDrawList();
        text(d, f.bold, add(p, S(18), S(16)), WHITE, title);
        if (badge) right(d, f.tiny, p.x + w - S(18), p.y + S(18), DIM, badge);
        line(d, add(p, S(1), S(48)), add(p, w - S(1), S(48)));
    }
    void separator(ImVec2 p, float w) const {
        line(ImGui::GetWindowDrawList(), p, add(p, w, 0));
    }

    bool button(const char* id, ImVec2 p, ImVec2 size, const char* label, bool primary = false) const {
        ImGui::SetCursorScreenPos(p);
        const bool pressed = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
        const bool hovered = ImGui::IsItemHovered();
        auto* d = ImGui::GetWindowDrawList();
        ImU32 bg = primary ? (hovered ? ORANGE_LIGHT : ORANGE) : (hovered ? IM_COL32(42, 42, 42, 255) : IM_COL32(32, 33, 34, 255));
        d->AddRectFilled(p, add(p, size), bg, S(4));
        if (!primary) d->AddRect(p, add(p, size), BORDER, S(4), 0, S(1));
        centered(d, f.small, p.x + size.x * .5f, p.y + (size.y - f.small->FontSize) * .5f,
                 primary ? IM_COL32(28, 20, 15, 255) : WHITE, label);
        if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
            d->AddRect(add(p, S(-2), S(-2)), add(add(p, size), S(2), S(2)), ORANGE, S(5), 0, S(1));
        return pressed;
    }

    void toggle(const char* id, ImVec2 p, float w, const char* label, bool& value,
                const char* description = nullptr) const {
        const float height = S(description ? 49.0f : 32.0f);
        ImGui::SetCursorScreenPos(p);
        if (ImGui::InvisibleButton(id, ImVec2(w, height), ImGuiButtonFlags_EnableNav)) value = !value;
        const bool hovered = ImGui::IsItemHovered();
        auto* d = ImGui::GetWindowDrawList();
        if (hovered) d->AddRectFilled(add(p, S(-7), S(1)), add(p, w + S(7), height - S(1)), IM_COL32(255,255,255,6), S(4));
        const float ty = p.y + (description ? S(4) : (height - f.regular->FontSize) * 0.5f);
        text(d, f.regular, ImVec2(p.x, ty), WHITE, label);
        if (description) text(d, f.small, add(p, 0, S(26)), DIM, description);
        const ImVec2 a(p.x + w - S(34), p.y + (height - S(19)) * 0.5f);
        d->AddRectFilled(a, add(a, S(34), S(19)), value ? ORANGE : IM_COL32(53, 55, 55, 255), S(10));
        d->AddCircleFilled(add(a, S(value ? 24.5f : 9.5f), S(9.5f)), S(6.3f), value ? IM_COL32(35, 24, 18, 255) : IM_COL32(145, 146, 145, 255), 20);
        if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
            d->AddRect(add(p, S(-3), 0), add(p, w + S(3), height), ORANGE, S(4), 0, S(1));
    }

    void combo(const char* id, ImVec2 p, float w, const char* label, int& current,
               const char* const* choices, int count) const {
        auto* d = ImGui::GetWindowDrawList();
        text(d, f.regular, add(p, 0, S(7)), WHITE, label);
        const float control_w = std::min(S(164), w * 0.49f);
        ImGui::SetCursorScreenPos(add(p, w - control_w, 0));
        ImGui::SetNextItemWidth(control_w);
        ImGui::PushFont(f.small);
        current = std::clamp(current, 0, count - 1);
        if (ImGui::BeginCombo(id, choices[current], ImGuiComboFlags_HeightLargest)) {
            for (int i = 0; i < count; ++i) {
                const bool selected = i == current;
                if (ImGui::Selectable(choices[i], selected)) current = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopFont();
    }

    void slider(const char* id, ImVec2 p, float w, const char* label, float& value,
                float min, float max, const char* suffix, int precision = 0) const {
        auto* d = ImGui::GetWindowDrawList();
        text(d, f.regular, p, WHITE, label);
        char number[48];
        if (precision == 1) std::snprintf(number, sizeof(number), "%.1f%s", static_cast<double>(value), suffix);
        else std::snprintf(number, sizeof(number), "%.0f%s", static_cast<double>(value), suffix);
        right(d, f.mono, p.x + w, p.y + S(1), ORANGE_LIGHT, number);
        const ImVec2 slider_pos = add(p, 0, S(22));
        ImGui::SetCursorScreenPos(slider_pos);
        ImGui::SetNextItemWidth(w);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
        ImGui::SliderFloat(id, &value, min, max, "", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
        const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        const float left = a.x + S(5), right_x = b.x - S(5), middle_y = (a.y + b.y) * 0.5f;
        const float t = std::clamp((value - min) / (max - min), 0.0f, 1.0f);
        const float point = left + (right_x - left) * t;
        d->AddRectFilled(ImVec2(left, middle_y - S(2)), ImVec2(right_x, middle_y + S(2)), IM_COL32(57, 58, 58, 255), S(2));
        d->AddRectFilled(ImVec2(left, middle_y - S(2)), ImVec2(point, middle_y + S(2)), ORANGE, S(2));
        d->AddCircleFilled(ImVec2(point, middle_y), S(5), ORANGE, 20);
        d->AddCircleFilled(ImVec2(point, middle_y), S(2), IM_COL32(35, 23, 17, 255), 12);
        if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
            d->AddRect(add(a, S(-2), S(-2)), add(b, S(2), S(2)), ORANGE, S(3), 0, S(1));
    }

    void color_row(const char* id, ImVec2 p, float w, const char* label, const char* detail, float color[4]) const {
        auto* d = ImGui::GetWindowDrawList();
        text(d, f.regular, add(p, 0, S(5)), WHITE, label);
        right(d, f.small, p.x + w - S(44), p.y + S(7), DIM, detail);
        ImGui::SetCursorScreenPos(add(p, w - S(28), 0));
        ImGui::PushFont(f.small);
        ImGui::ColorEdit4(id, color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                                    ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip);
        ImGui::PopFont();
    }

    void icon(ImDrawList* d, int which, ImVec2 c, ImU32 col) const {
        const float t = S(1.5f), r = S(6.5f);
        if (which == 0) {
            d->AddCircle(c, r, col, 24, t);
            d->AddCircleFilled(c, S(1.5f), col, 12);
            line(d, add(c, -S(10), 0), add(c, -S(5), 0), col, 1.5f);
            line(d, add(c, S(5), 0), add(c, S(10), 0), col, 1.5f);
            line(d, add(c, 0, -S(10)), add(c, 0, -S(5)), col, 1.5f);
            line(d, add(c, 0, S(5)), add(c, 0, S(10)), col, 1.5f);
        } else if (which == 1) {
            d->PathLineTo(add(c, -S(9), 0));
            d->PathBezierCubicCurveTo(add(c, -S(4), -S(7)), add(c, S(4), -S(7)), add(c, S(9), 0));
            d->PathBezierCubicCurveTo(add(c, S(4), S(7)), add(c, -S(4), S(7)), add(c, -S(9), 0));
            d->PathStroke(col, ImDrawFlags_Closed, t);
            d->AddCircle(c, S(2.4f), col, 16, t);
        } else if (which == 2) {
            d->AddQuad(add(c, 0, -S(9)), add(c, S(8), 0), add(c, 0, S(9)), add(c, -S(8), 0), col, t);
            d->AddCircleFilled(c, S(2), col, 12);
        } else if (which == 3) {
            for (int i = 0; i < 3; ++i) {
                const float y = c.y + S((i - 1) * 5.5f);
                line(d, ImVec2(c.x - S(8), y), ImVec2(c.x + S(8), y), col, 1.5f);
                const float x = c.x + S(i == 1 ? 3 : -3);
                d->AddCircleFilled(ImVec2(x, y), S(2.2f), col, 12);
            }
        } else {
            d->AddRect(add(c, -S(6), -S(6)), add(c, S(6), S(6)), col, S(1), 0, t);
            d->AddCircleFilled(c, S(2), col, 12);
            for (int i = -1; i <= 1; ++i) {
                line(d, add(c, S(i * 4.0f), -S(9)), add(c, S(i * 4.0f), -S(6)), col, 1.2f);
                line(d, add(c, S(i * 4.0f), S(6)), add(c, S(i * 4.0f), S(9)), col, 1.2f);
            }
        }
    }

    void sidebar(float height) {
        auto* d = ImGui::GetWindowDrawList();
        d->AddRectFilled(ImVec2(0, 0), V(218, height / scale), SIDEBAR);
        line(d, V(218, 0), V(218, height / scale), BORDER_SUBTLE);
        // A small geometric mark, drawn entirely with ImGui primitives.
        const ImVec2 mark = V(23, 24);
        d->AddRectFilled(mark, add(mark, S(32), S(32)), ORANGE, S(5));
        d->AddLine(add(mark, S(9), S(8)), add(mark, S(9), S(24)), IM_COL32(30,22,18,255), S(2.5f));
        d->AddLine(add(mark, S(10), S(16)), add(mark, S(22), S(8)), IM_COL32(30,22,18,255), S(2.5f));
        d->AddLine(add(mark, S(10), S(16)), add(mark, S(23), S(24)), IM_COL32(30,22,18,255), S(2.5f));
        text(d, f.brand, V(66, 23), WHITE, "KINGWARE");
        text(d, f.tiny, V(67, 46), DIM, "EXTERNAL  /  RUST");
        line(d, V(0, 80), V(218, 80));
        text(d, f.tiny, V(23, 109), DIM, "НАВИГАЦИЯ");
        const char* labels[] = {"Aimbot", "Visuals", "Resolver", "Miscellaneous", "Debug"};
        const Page pages[] = {Page::Aimbot, Page::Visuals, Page::Resolver, Page::Misc, Page::Debug};
        for (int i = 0; i < 5; ++i) {
            const ImVec2 p = V(12, 133 + i * 48.0f);
            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##nav", V(194, 40), ImGuiButtonFlags_EnableNav)) state.page = pages[i];
            const bool hovered = ImGui::IsItemHovered();
            const bool selected = state.page == pages[i];
            if (selected) {
                d->AddRectFilled(p, add(p, S(194), S(40)), ORANGE_DARK, S(4));
                d->AddRectFilled(add(p, 0, S(8)), add(p, S(3), S(32)), ORANGE, S(2));
            } else if (hovered) {
                d->AddRectFilled(p, add(p, S(194), S(40)), IM_COL32(33, 34, 34, 255), S(4));
            }
            icon(d, i, add(p, S(20), S(20)), selected ? ORANGE : MUTED);
            text(d, f.regular, add(p, S(41), S(12)), selected ? WHITE : MUTED, labels[i]);
            if (selected) {
                d->AddCircleFilled(add(p, S(178), S(20)), S(2), ORANGE, 12);
            }
            if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
                d->AddRect(p, add(p, S(194), S(40)), ORANGE, S(4), 0, S(1));
            ImGui::PopID();
        }
        const float bottom = height - S(139);
        d->AddRectFilled(ImVec2(S(12), bottom), ImVec2(S(206), height - S(18)), IM_COL32(24,25,25,255), S(5));
        d->AddRect(ImVec2(S(12), bottom), ImVec2(S(206), height - S(18)), BORDER, S(5), 0, S(1));
        text(d, f.tiny, ImVec2(S(26), bottom + S(14)), DIM, "СОСТОЯНИЕ СИСТЕМЫ");
        dot(d, ImVec2(S(29), bottom + S(46)), state.driver_state == DriverState::Connected ? GREEN : ORANGE, 3);
        text(d, f.small, ImVec2(S(39), bottom + S(39)), WHITE, "Драйвер");
        right(d, f.tiny, S(191), bottom + S(41), state.driver_state == DriverState::Connected ? GREEN : ORANGE_LIGHT, driver_label(state.driver_state));
        dot(d, ImVec2(S(29), bottom + S(73)), DIM, 3);
        text(d, f.small, ImVec2(S(39), bottom + S(66)), MUTED, "Rust");
        right(d, f.tiny, S(191), bottom + S(68), DIM, "НЕ ПРОВЕРЕН");
        line(d, ImVec2(S(26), bottom + S(91)), ImVec2(S(192), bottom + S(91)));
        text(d, f.tiny, ImVec2(S(26), bottom + S(100)), DIM, "ТОЛЬКО ИНТЕРФЕЙС / 0.1");
    }

    void chrome(float width, float height) {
        auto* d = ImGui::GetWindowDrawList();
        d->AddRectFilled(ImVec2(S(218), 0), ImVec2(width, S(64)), SIDEBAR);
        line(d, ImVec2(S(218), S(64)), ImVec2(width, S(64)));
        text(d, f.tiny, V(249, 25), DIM, "КОНТРОЛЬНАЯ ПАНЕЛЬ");
        text(d, f.tiny, V(382, 25), DIM, "/");
        text(d, f.tiny, V(397, 25), ORANGE_LIGHT, page_name(state.page));
        const float badge_x = width - S(313);
        d->AddRectFilled(ImVec2(badge_x, S(21)), ImVec2(badge_x + S(101), S(43)), ORANGE_DARK, S(3));
        dot(d, ImVec2(badge_x + S(11), S(32)), ORANGE, 2.5f);
        text(d, f.tiny, ImVec2(badge_x + S(20), S(26)), ORANGE_LIGHT, "DEMO MODE");
        if (button("##save-top", ImVec2(width - S(196), S(17)), V(102, 30), "СОХРАНИТЬ")) state.request_save = true;
        if (button("##min", ImVec2(width - S(79), S(17)), V(28, 30), "-")) state.request_minimize = true;
        if (button("##close", ImVec2(width - S(45), S(17)), V(28, 30), "×")) state.request_close = true;
        line(d, ImVec2(S(218), height - S(32)), ImVec2(width, height - S(32)));
        dot(d, ImVec2(S(251), height - S(16)), ORANGE, 2.5f);
        text(d, f.tiny, ImVec2(S(260), height - S(22)), MUTED, "GUI ONLY  /  НЕТ ПОДКЛЮЧЕНИЯ К RUST");
        const std::string profile = std::string("ПРОФИЛЬ: ") + profile_label(state.profile_status);
        right(d, f.tiny, width - S(31), height - S(22), state.profile_status == ProfileStatus::Error ? ORANGE_LIGHT : DIM, profile.c_str());
        if (state.settings.misc_fps) {
            char fps[32];
            std::snprintf(fps, sizeof(fps), "%.0f FPS", static_cast<double>(ImGui::GetIO().Framerate));
            right(d, f.tiny, width - S(215), height - S(22), DIM, fps);
        }
        d->AddRect(ImVec2(0,0), ImVec2(width, height), BORDER, 0.0f, 0, S(1));
    }

    void page_title(float width) {
        auto* d = ImGui::GetWindowDrawList();
        const char* title = "Визуал";
        const char* description = "Настройте отображение игроков в предпросмотре.";
        const char* index = "02  /  VISUALS";
        switch (state.page) {
        case Page::Aimbot: title = "Наведение"; description = "Параметры выбора цели и плавности движения."; index = "01  /  AIMBOT"; break;
        case Page::Visuals: break;
        case Page::Resolver: title = "Resolver"; description = "Конфигурация коррекции и прогнозирования."; index = "03  /  RESOLVER"; break;
        case Page::Misc: title = "Настройки"; description = "Интерфейс и локальный профиль приложения."; index = "04  /  MISCELLANEOUS"; break;
        case Page::Debug: title = "Диагностика"; description = "Состояние интеграции и переданные offsets."; index = "05  /  DEBUG"; break;
        }
        text(d, f.heading, V(249, 84), WHITE, title);
        text(d, f.regular, V(250, 121), MUTED, description);
        right(d, f.tiny, width - S(32), S(101), DIM, index);
    }

    void visual_preview(ImVec2 p, float w) {
        card(p, w, 430);
        card_header(p, w, "ПРЕДПРОСМОТР");
        if (button("##visual-visibility", add(p, w - S(111), S(10)), V(94, 28),
                   state.demo_visible ? "ВИДЕН" : "СКРЫТ")) state.demo_visible = !state.demo_visible;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Переключить видимость демонстрационной модели");
        auto* d = ImGui::GetWindowDrawList();
        const ImVec2 a = add(p, S(15), S(61));
        const float scene_w = w - S(30), scene_h = S(299);
        const ImVec2 b = add(a, scene_w, scene_h);
        d->AddRectFilled(a, b, IM_COL32(17, 19, 19, 255), S(4));
        d->PushClipRect(a, b, true);
        // Abstract grid and mannequin, not a game screenshot.
        for (float x = a.x + S(24); x < b.x; x += S(24)) line(d, ImVec2(x, a.y), ImVec2(x, b.y), IM_COL32(31,34,34,255));
        for (float y = a.y + S(24); y < b.y; y += S(24)) line(d, ImVec2(a.x, y), ImVec2(b.x, y), IM_COL32(31,34,34,255));
        d->AddRectFilledMultiColor(a, b, IM_COL32(22,24,23,150), IM_COL32(22,24,23,150), IM_COL32(15,16,17,200), IM_COL32(15,16,17,200));
        text(d, f.tiny, add(a, S(12), S(11)), DIM, "01  /  ТЕСТОВАЯ МОДЕЛЬ");
        right(d, f.tiny, b.x - S(12), a.y + S(11), ORANGE_LIGHT, "DEMO");
        if (state.settings.misc_watermark)
            text(d, f.tiny, ImVec2(a.x + S(12), b.y - S(18)), DIM, "KINGWARE / ПРЕВЬЮ");

        const float cx = (a.x + b.x) * 0.5f;
        const float top = a.y + S(44);
        const bool drawing = state.settings.visual_enabled && state.settings.visual_distance_max >= 78.0f;
        const float* color = state.demo_visible ? state.settings.visual_color : state.settings.visual_hidden_color;
        const ImU32 active = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], 1.0f));
        const int chams = drawing ? state.settings.visual_chams : 0;
        const ImU32 body = chams == 1 ? with_alpha(active, 180) : IM_COL32(91, 99, 96, 255);
        const ImU32 shade = chams == 1 ? with_alpha(active, 145) : IM_COL32(68, 74, 72, 255);
        const ImU32 edge = (chams == 2 || chams == 3) ? active : IM_COL32(128, 136, 130, 255);
        // Limbs first, then torso/head to make a readable silhouette.
        d->AddLine(ImVec2(cx - S(17), top + S(112)), ImVec2(cx - S(21), top + S(183)), shade, S(15));
        d->AddLine(ImVec2(cx + S(17), top + S(112)), ImVec2(cx + S(22), top + S(183)), shade, S(15));
        d->AddLine(ImVec2(cx - S(22), top + S(179)), ImVec2(cx - S(26), top + S(194)), body, S(12));
        d->AddLine(ImVec2(cx + S(22), top + S(179)), ImVec2(cx + S(27), top + S(194)), body, S(12));
        d->AddLine(ImVec2(cx - S(24), top + S(50)), ImVec2(cx - S(39), top + S(112)), shade, S(12));
        d->AddLine(ImVec2(cx + S(24), top + S(50)), ImVec2(cx + S(39), top + S(112)), shade, S(12));
        ImVec2 torso[] = {
            ImVec2(cx - S(22), top + S(39)), ImVec2(cx + S(22), top + S(39)),
            ImVec2(cx + S(30), top + S(62)), ImVec2(cx + S(23), top + S(116)),
            ImVec2(cx - S(23), top + S(116)), ImVec2(cx - S(30), top + S(62))
        };
        d->AddConvexPolyFilled(torso, 6, body);
        d->AddPolyline(torso, 6, edge, ImDrawFlags_Closed, S(1));
        d->AddRectFilled(ImVec2(cx - S(18), top + S(58)), ImVec2(cx + S(18), top + S(98)), shade, S(2));
        d->AddLine(ImVec2(cx - S(18), top + S(79)), ImVec2(cx + S(18), top + S(79)), edge, S(1));
        d->AddCircleFilled(ImVec2(cx, top + S(20)), S(13), body, 30);
        d->AddCircle(ImVec2(cx, top + S(20)), S(13), edge, 30, S(1));
        d->AddLine(ImVec2(cx - S(9), top + S(20)), ImVec2(cx + S(9), top + S(20)), shade, S(2));
        if (chams == 2) {
            d->AddCircle(ImVec2(cx, top + S(20)), S(16), active, 30, S(1));
            d->AddLine(ImVec2(cx - S(29), top + S(48)), ImVec2(cx - S(44), top + S(112)), active, S(1));
            d->AddLine(ImVec2(cx + S(29), top + S(48)), ImVec2(cx + S(44), top + S(112)), active, S(1));
        }
        if (drawing && (state.settings.visual_skeleton || chams == 3)) {
            const ImU32 skel = chams == 3 ? active : IM_COL32(236, 238, 229, 220);
            const ImVec2 head(cx, top + S(20)), neck(cx, top + S(41)), hip(cx, top + S(107));
            line(d, head, neck, skel, 1.2f); line(d, neck, hip, skel, 1.2f);
            line(d, neck, ImVec2(cx - S(24), top + S(51)), skel, 1.2f);
            line(d, neck, ImVec2(cx + S(24), top + S(51)), skel, 1.2f);
            line(d, ImVec2(cx - S(24), top + S(51)), ImVec2(cx - S(39), top + S(112)), skel, 1.2f);
            line(d, ImVec2(cx + S(24), top + S(51)), ImVec2(cx + S(39), top + S(112)), skel, 1.2f);
            line(d, hip, ImVec2(cx - S(21), top + S(183)), skel, 1.2f);
            line(d, hip, ImVec2(cx + S(22), top + S(183)), skel, 1.2f);
            dot(d, head, skel, 2); dot(d, neck, skel, 2); dot(d, hip, skel, 2);
        }
        const ImVec2 box_a(cx - S(51), top - S(2));
        const ImVec2 box_b(cx + S(51), top + S(201));
        if (drawing) {
            if (state.settings.visual_box == 2) {
                d->AddRect(box_a, box_b, active, 0, 0, S(1.5f));
            } else if (state.settings.visual_box == 1) {
                const float x = S(15), y = S(18);
                line(d, box_a, add(box_a, x, 0), active, 1.5f); line(d, box_a, add(box_a, 0, y), active, 1.5f);
                line(d, ImVec2(box_b.x, box_a.y), ImVec2(box_b.x - x, box_a.y), active, 1.5f);
                line(d, ImVec2(box_b.x, box_a.y), ImVec2(box_b.x, box_a.y + y), active, 1.5f);
                line(d, ImVec2(box_a.x, box_b.y), ImVec2(box_a.x + x, box_b.y), active, 1.5f);
                line(d, ImVec2(box_a.x, box_b.y), ImVec2(box_a.x, box_b.y - y), active, 1.5f);
                line(d, box_b, add(box_b, -x, 0), active, 1.5f); line(d, box_b, add(box_b, 0, -y), active, 1.5f);
            }
            if (state.settings.visual_health) {
                d->AddRectFilled(ImVec2(box_a.x - S(8), box_a.y), ImVec2(box_a.x - S(4), box_b.y), IM_COL32(18, 23, 18, 255));
                d->AddRectFilled(ImVec2(box_a.x - S(8), box_b.y - (box_b.y-box_a.y)*.78f), ImVec2(box_a.x - S(4), box_b.y), GREEN);
            }
            if (state.settings.visual_armor) {
                d->AddRectFilled(ImVec2(box_b.x + S(4), box_a.y), ImVec2(box_b.x + S(8), box_b.y), IM_COL32(19, 22, 26, 255));
                d->AddRectFilled(ImVec2(box_b.x + S(4), box_b.y - (box_b.y-box_a.y)*.62f), ImVec2(box_b.x + S(8), box_b.y), BLUE);
            }
            if (state.settings.visual_name) centered(d, f.small, cx, top - S(18), WHITE, "WANDERER");
            if (state.settings.visual_weapon) centered(d, f.tiny, cx, box_b.y + S(4), WHITE, "ASSAULT RIFLE");
            if (state.settings.visual_distance) centered(d, f.tiny, cx, box_b.y + S(state.settings.visual_weapon ? 18 : 4), MUTED, "78 M");
        }
        if (state.settings.misc_crosshair) {
            line(d, ImVec2(cx - S(7), a.y + scene_h * .5f), ImVec2(cx + S(7), a.y + scene_h * .5f), WHITE);
            line(d, ImVec2(cx, a.y + scene_h * .5f - S(7)), ImVec2(cx, a.y + scene_h * .5f + S(7)), WHITE);
        }
        d->PopClipRect();
        d->AddRect(a, b, BORDER_SUBTLE, S(4), 0, S(1));
        dot(d, add(p, S(20), S(385)), ORANGE, 3);
        text(d, f.small, add(p, S(30), S(378)), WHITE, "Демонстрационные данные");
        text(d, f.tiny, add(p, S(30), S(398)), DIM,
             state.settings.visual_distance_max < 78.0f ? "ЦЕЛЬ ЗА ПРЕДЕЛАМИ ДИСТАНЦИИ" : "НАСТРОЙКИ ПРИМЕНЯЮТСЯ СРАЗУ");
    }

    void visual_left(ImVec2 p, float w) {
        const float pad = S(18), inner = w - pad * 2;
        card(p, w, 440);
        card_header(p, w, "ИГРОКИ", "PLAYER ESP");
        toggle("##visual-enabled", add(p, pad, S(56)), inner, "ESP игроков", state.settings.visual_enabled, "Параметры отображения модели");
        separator(add(p, pad, S(112)), inner);
        const char* boxes[] = {"Выключен", "Уголки", "Полный"};
        combo("##box-type", add(p, pad, S(124)), inner, "Тип бокса", state.settings.visual_box, boxes, 3);
        toggle("##health", add(p, pad, S(164)), inner, "Здоровье", state.settings.visual_health);
        toggle("##armor", add(p, pad, S(198)), inner, "Броня", state.settings.visual_armor);
        toggle("##name", add(p, pad, S(232)), inner, "Имя игрока", state.settings.visual_name);
        toggle("##weapon", add(p, pad, S(266)), inner, "Оружие", state.settings.visual_weapon);
        toggle("##distance", add(p, pad, S(300)), inner, "Дистанция", state.settings.visual_distance);
        toggle("##skeleton", add(p, pad, S(334)), inner, "Скелет", state.settings.visual_skeleton);
        separator(add(p, pad, S(370)), inner);
        slider("##visual-max", add(p, pad, S(382)), inner, "Макс. дистанция", state.settings.visual_distance_max, 50, 1000, " м");

        const ImVec2 q = add(p, 0, S(452));
        card(q, w, 124);
        card_header(q, w, "CHAMS", "ТОЛЬКО ПРЕВЬЮ");
        const char* modes[] = {"Отключены", "Заливка", "Контур", "Сетка"};
        combo("##chams", add(q, pad, S(59)), inner, "Стиль модели", state.settings.visual_chams, modes, 4);
        text(ImGui::GetWindowDrawList(), f.small, add(q, pad, S(96)), DIM, "В игре отрисовка не подключена");
    }

    void visual_right(ImVec2 p, float w) {
        visual_preview(p, w);
        const ImVec2 q = add(p, 0, S(442));
        card(q, w, 134);
        card_header(q, w, "ЦВЕТА", "ESP / CHAMS");
        const float pad = S(18), inner = w - pad * 2;
        color_row("##visible-color", add(q, pad, S(57)), inner, "Видимый", "ОСНОВНОЙ", state.settings.visual_color);
        color_row("##hidden-color", add(q, pad, S(93)), inner, "Скрытый", "ЗА ПРЕПЯТСТВИЕМ", state.settings.visual_hidden_color);
    }

    void aim_preview(ImVec2 p, float w) {
        card(p, w, 430);
        card_header(p, w, "СИМУЛЯЦИЯ НАВЕДЕНИЯ");
        if (button("##aim-visibility", add(p, w - S(111), S(10)), V(94, 28),
                   state.demo_visible ? "ВИДЕН" : "СКРЫТ")) state.demo_visible = !state.demo_visible;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Переключить видимость тестовой цели");
        auto* d = ImGui::GetWindowDrawList();
        const ImVec2 a = add(p, S(15), S(61));
        const float scene_w = w - S(30), scene_h = S(299);
        const ImVec2 b = add(a, scene_w, scene_h);
        d->AddRectFilled(a, b, IM_COL32(17,19,19,255), S(4));
        d->PushClipRect(a, b, true);
        for (float x = a.x + S(24); x < b.x; x += S(24)) line(d, ImVec2(x, a.y), ImVec2(x, b.y), IM_COL32(31,34,34,255));
        for (float y = a.y + S(24); y < b.y; y += S(24)) line(d, ImVec2(a.x, y), ImVec2(b.x, y), IM_COL32(31,34,34,255));
        text(d, f.tiny, add(a, S(12), S(11)), DIM, "ПРОЕКЦИЯ  /  ДЕМО");
        right(d, f.tiny, b.x - S(12), a.y + S(11), ORANGE_LIGHT, "НЕ ИГРА");
        if (state.settings.misc_watermark)
            text(d, f.tiny, ImVec2(a.x + S(12), b.y - S(18)), DIM, "KINGWARE / ПРЕВЬЮ");
        const ImVec2 center((a.x + b.x) * .5f, (a.y + b.y) * .5f + S(4));
        const float seconds = static_cast<float>(ImGui::GetTime());
        const float tx = 50.0f + 12.0f * std::sin(seconds * .8f);
        const float ty = -35.0f + 8.0f * std::sin(seconds * .55f);
        aim::Target demo{1, true, false, 78.0f, {
            {aim::Bone::Head, {tx, ty}, 10.0f, state.demo_visible},
            {aim::Bone::Neck, {tx, ty + 16.0f}, 6.0f, state.demo_visible},
            {aim::Bone::Chest, {tx, ty + 36.0f}, 15.0f, state.demo_visible}}};
        aim::Options opts;
        opts.enabled = state.settings.aim_enabled;
        opts.visible_only = state.settings.aim_visible_only;
        opts.ignore_team = state.settings.aim_ignore_team;
        opts.multipoint = state.settings.aim_multipoint;
        opts.bone = static_cast<aim::Bone>(std::clamp(state.settings.aim_bone, 0, 3));
        opts.fov_radius = state.settings.aim_fov;
        opts.point_scale = state.settings.aim_point_scale / 100.0f;
        opts.smooth = state.settings.aim_smooth;
        opts.humanization = state.settings.aim_humanization / 100.0f;
        if (!opts.enabled) preview_cursor = {};
        const auto solution = aim::solve(preview_cursor, {demo}, opts, ImGui::GetIO().DeltaTime, seconds);
        if (solution) {
            preview_cursor.x += solution->delta.x;
            preview_cursor.y += solution->delta.y;
        }
        const ImVec2 cursor = add(center, S(preview_cursor.x), S(preview_cursor.y));
        const ImVec2 target = add(center, S(tx), S(ty));
        if (state.settings.aim_draw_fov) {
            d->AddCircle(cursor, S(state.settings.aim_fov), IM_COL32(255,119,55,90), 96, S(1));
            d->AddCircle(cursor, S(state.settings.aim_fov) + S(1), IM_COL32(255,119,55,15), 96, S(2));
        }
        d->AddCircle(target, S(10), state.demo_visible ? ORANGE : DIM, 32, S(1.3f));
        d->AddCircleFilled(target, S(2.5f), state.demo_visible ? ORANGE : DIM, 16);
        if (state.settings.aim_multipoint) {
            const float r = S(10.0f * opts.point_scale);
            dot(d, add(target, -r, 0), ORANGE_LIGHT, 1.6f); dot(d, add(target, r, 0), ORANGE_LIGHT, 1.6f);
            dot(d, add(target, 0, -r), ORANGE_LIGHT, 1.6f); dot(d, add(target, 0, r), ORANGE_LIGHT, 1.6f);
        }
        if (solution) line(d, cursor, add(center, S(solution->point.x), S(solution->point.y)), IM_COL32(255,119,55,105));
        line(d, add(cursor, -S(11), 0), add(cursor, -S(3), 0), WHITE, 1.4f);
        line(d, add(cursor, S(3), 0), add(cursor, S(11), 0), WHITE, 1.4f);
        line(d, add(cursor, 0, -S(11)), add(cursor, 0, -S(3)), WHITE, 1.4f);
        line(d, add(cursor, 0, S(3)), add(cursor, 0, S(11)), WHITE, 1.4f);
        dot(d, cursor, ORANGE, 1.8f);
        d->PopClipRect();
        d->AddRect(a, b, BORDER_SUBTLE, S(4), 0, S(1));
        dot(d, add(p, S(20), S(385)), solution ? GREEN : ORANGE, 3);
        text(d, f.small, add(p, S(30), S(378)), WHITE, solution ? "Тестовая цель выбрана" : "Тестовая цель не выбрана");
        text(d, f.tiny, add(p, S(30), S(398)), DIM, "ДВИЖЕНИЕ МЫШИ И ИГРА НЕ УПРАВЛЯЮТСЯ");
    }

    void aim_left(ImVec2 p, float w) {
        const float pad = S(18), inner = w - 2 * pad;
        card(p, w, 315);
        card_header(p, w, "ВЫБОР ЦЕЛИ", "НАСТРОЙКИ");
        toggle("##aim-enabled", add(p, pad, S(55)), inner, "Aimbot", state.settings.aim_enabled, "Включить расчёт в симуляции");
        separator(add(p, pad, S(109)), inner);
        toggle("##visible-only", add(p, pad, S(116)), inner, "Только видимые", state.settings.aim_visible_only);
        toggle("##ignore-team", add(p, pad, S(150)), inner, "Игнорировать команду", state.settings.aim_ignore_team);
        toggle("##multipoint", add(p, pad, S(184)), inner, "Multipoint", state.settings.aim_multipoint);
        const char* bones[] = {"Голова", "Шея", "Грудь", "Ближайшая"};
        const char* keys[] = {"Правая кнопка", "Mouse 4", "Left Shift", "Left Alt"};
        combo("##aim-bone", add(p, pad, S(225)), inner, "Точка наведения", state.settings.aim_bone, bones, 4);
        combo("##aim-key", add(p, pad, S(267)), inner, "Клавиша активации", state.settings.aim_key, keys, 4);

        const ImVec2 q = add(p, 0, S(327));
        card(q, w, 249);
        card_header(q, w, "ПЛАВНОСТЬ", "AIM BEHAVIOR");
        slider("##fov", add(q, pad, S(58)), inner, "Радиус FOV", state.settings.aim_fov, 1, 180, " px");
        slider("##smooth", add(q, pad, S(109)), inner, "Smooth", state.settings.aim_smooth, 1, 30, "x", 1);
        slider("##human", add(q, pad, S(160)), inner, "Humanization", state.settings.aim_humanization, 0, 100, "%");
        // Multipoint scale is configurable without expanding this card; see the right panel.
    }

    void aim_right(ImVec2 p, float w) {
        aim_preview(p, w);
        const ImVec2 q = add(p, 0, S(442));
        card(q, w, 134);
        card_header(q, w, "ДОПОЛНИТЕЛЬНО", "DEMO ONLY");
        const float pad = S(18), inner = w - 2 * pad;
        toggle("##draw-fov", add(q, pad, S(54)), inner, "Показывать FOV", state.settings.aim_draw_fov);
        slider("##point-scale", add(q, pad, S(91)), inner, "Размер multipoint", state.settings.aim_point_scale, 0, 100, "%");
    }

    void resolver_left(ImVec2 p, float w) {
        const float pad = S(18), inner = w - 2 * pad;
        card(p, w, 283);
        card_header(p, w, "КОРРЕКЦИЯ", "CONFIG ONLY");
        toggle("##resolver-enabled", add(p, pad, S(55)), inner, "Resolver", state.settings.resolver_enabled, "Сохранить параметры коррекции");
        separator(add(p, pad, S(110)), inner);
        toggle("##resolver-prediction", add(p, pad, S(119)), inner, "Прогноз скорости", state.settings.resolver_prediction);
        toggle("##resolver-interpolation", add(p, pad, S(156)), inner, "Интерполяция", state.settings.resolver_interpolation);
        const char* modes[] = {"Адаптивный", "Скорость", "Интерполяция"};
        combo("##resolver-mode", add(p, pad, S(204)), inner, "Режим коррекции", state.settings.resolver_mode, modes, 3);
        text(ImGui::GetWindowDrawList(), f.small, add(p, pad, S(251)), DIM, "Алгоритм для Rust пока не подключён");

        const ImVec2 q = add(p, 0, S(295));
        card(q, w, 210);
        card_header(q, w, "ПАРАМЕТРЫ", "PREDICTION");
        slider("##resolver-strength", add(q, pad, S(60)), inner, "Сила коррекции", state.settings.resolver_strength, 0, 100, "%");
        slider("##resolver-time", add(q, pad, S(116)), inner, "Макс. время прогноза", state.settings.resolver_max_time, 0, 300, " ms");
        text(ImGui::GetWindowDrawList(), f.small, add(q, pad, S(177)), DIM, "Значения сохраняются в профиль");
    }

    void status_line(ImVec2 p, float w, const char* label, const char* value, ImU32 color = DIM) const {
        auto* d = ImGui::GetWindowDrawList();
        text(d, f.regular, p, MUTED, label);
        right(d, f.small, p.x + w, p.y + S(1), color, value);
    }

    void resolver_right(ImVec2 p, float w) {
        card(p, w, 376);
        card_header(p, w, "ИСТОЧНИК ДАННЫХ", "ОЖИДАНИЕ");
        auto* d = ImGui::GetWindowDrawList();
        const ImVec2 a = add(p, S(17), S(64));
        const ImVec2 b = add(a, w - S(34), S(158));
        d->AddRectFilled(a, b, IM_COL32(19,21,21,255), S(4));
        d->AddRect(a, b, BORDER_SUBTLE, S(4), 0, S(1));
        for (float x = a.x + S(24); x < b.x; x += S(24)) line(d, ImVec2(x, a.y), ImVec2(x, b.y), IM_COL32(30,33,33,255));
        for (float y = a.y + S(24); y < b.y; y += S(24)) line(d, ImVec2(a.x, y), ImVec2(b.x, y), IM_COL32(30,33,33,255));
        const float cx = (a.x+b.x)*.5f, cy = (a.y+b.y)*.5f;
        d->AddCircle(ImVec2(cx,cy), S(42), IM_COL32(78,60,47,255), 48, S(1));
        d->AddCircle(ImVec2(cx,cy), S(27), IM_COL32(99,68,49,255), 48, S(1));
        line(d, ImVec2(cx-S(60),cy), ImVec2(cx+S(60),cy), IM_COL32(86,64,49,255));
        line(d, ImVec2(cx,cy-S(58)), ImVec2(cx,cy+S(58)), IM_COL32(86,64,49,255));
        dot(d, ImVec2(cx,cy), ORANGE, 3);
        line(d, ImVec2(cx,cy), ImVec2(cx+S(34),cy-S(28)), ORANGE, 1.5f);
        d->AddTriangleFilled(ImVec2(cx+S(34),cy-S(28)), ImVec2(cx+S(25),cy-S(27)), ImVec2(cx+S(31),cy-S(19)), ORANGE);
        status_line(add(p, S(18), S(242)), w-S(36), "Данные позиции", "НЕТ ИСТОЧНИКА", ORANGE_LIGHT);
        status_line(add(p, S(18), S(275)), w-S(36), "Скорость цели", "НЕ ДОСТУПНА");
        status_line(add(p, S(18), S(308)), w-S(36), "Расчёт для игры", "НЕ ПОДКЛЮЧЁН");
        line(d, add(p, S(18), S(338)), add(p, w-S(18), S(338)));
        text(d, f.small, add(p, S(18), S(349)), DIM, "Драйвер и данные Rust необходимы отдельно.");
    }

    void misc_left(ImVec2 p, float w) {
        const float pad = S(18), inner = w - 2 * pad;
        card(p, w, 212);
        card_header(p, w, "ИНТЕРФЕЙС", "PREVIEW");
        toggle("##watermark", add(p, pad, S(58)), inner, "Watermark в превью", state.settings.misc_watermark);
        toggle("##fps", add(p, pad, S(98)), inner, "FPS интерфейса", state.settings.misc_fps);
        toggle("##crosshair", add(p, pad, S(138)), inner, "Прицел в превью", state.settings.misc_crosshair);
        text(ImGui::GetWindowDrawList(), f.small, add(p, pad, S(183)), DIM, "Внешний игровой оверлей не подключён");

        const ImVec2 q = add(p, 0, S(224));
        card(q, w, 188);
        card_header(q, w, "ПРОФИЛЬ", "ЛОКАЛЬНО");
        auto* d = ImGui::GetWindowDrawList();
        text(d, f.regular, add(q, pad, S(63)), WHITE, "Параметры приложения");
        text(d, f.small, add(q, pad, S(86)), DIM, "Изменения сохраняются автоматически");
        if (button("##save-profile", add(q, pad, S(119)), V(132, 33), "СОХРАНИТЬ", true)) state.request_save = true;
        if (button("##reset-profile", add(q, pad + S(142), S(119)), V(108, 33), "СБРОСИТЬ")) ImGui::OpenPopup("Сбросить настройки?");
        if (state.profile_status == ProfileStatus::Error) {
            text(d, f.small, add(q, pad, S(162)), ORANGE_LIGHT, "Ошибка профиля: подробности в Debug");
        }
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * .5f, ImGui::GetIO().DisplaySize.y * .5f), ImGuiCond_Appearing, ImVec2(.5f,.5f));
        if (ImGui::BeginPopupModal("Сбросить настройки?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Вернуть все параметры профиля к исходным?");
            ImGui::TextDisabled("Изменение будет сохранено локально.");
            ImGui::Spacing();
            if (ImGui::Button("Сбросить", V(110, 30))) {
                state.settings = Settings{};
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Отмена", V(110, 30))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void misc_right(ImVec2 p, float w) {
        card(p, w, 344);
        card_header(p, w, "О ПРИЛОЖЕНИИ", "KINGWARE / 0.1");
        auto* d = ImGui::GetWindowDrawList();
        const ImVec2 a = add(p, S(18), S(66));
        d->AddRectFilled(a, add(a, S(42), S(42)), ORANGE_DARK, S(5));
        icon(d, 3, add(a, S(21), S(21)), ORANGE);
        text(d, f.bold, add(p, S(73), S(65)), WHITE, "Внешний интерфейс");
        text(d, f.small, add(p, S(73), S(88)), DIM, "C++  /  DEAR IMGUI  /  DIRECTX 11");
        separator(add(p, S(18), S(130)), w - S(36));
        status_line(add(p, S(18), S(148)), w-S(36), "Режим", "DEMO", ORANGE_LIGHT);
        status_line(add(p, S(18), S(183)), w-S(36), "Профиль", profile_label(state.profile_status), state.profile_status == ProfileStatus::Error ? ORANGE_LIGHT : WHITE);
        status_line(add(p, S(18), S(218)), w-S(36), "Подключение Rust", "НЕТ", DIM);
        separator(add(p, S(18), S(254)), w - S(36));
        text(d, f.small, add(p, S(18), S(270)), MUTED, "Настройки влияют только на GUI и демо.");
        text(d, f.small, add(p, S(18), S(294)), DIM, "Путь профиля: %APPDATA%\\Kingware");
        text(d, f.small, add(p, S(18), S(315)), DIM, "Для подключения нужны исходники backend.");
    }

    void debug_left(ImVec2 p, float w) {
        card(p, w, 300);
        card_header(p, w, "СОСТОЯНИЕ", "BACKEND");
        auto* d = ImGui::GetWindowDrawList();
        status_line(add(p, S(18), S(67)), w-S(36), "Драйвер", driver_detail(state.driver_state), state.driver_state == DriverState::Connected ? GREEN : ORANGE_LIGHT);
        status_line(add(p, S(18), S(104)), w-S(36), "Процесс Rust", "НЕ ПРОВЕРЕН");
        status_line(add(p, S(18), S(141)), w-S(36), "Матрица камеры", "НЕТ ДАННЫХ");
        status_line(add(p, S(18), S(178)), w-S(36), "Aimbot / ESP", "ТОЛЬКО ДЕМО", ORANGE_LIGHT);
        status_line(add(p, S(18), S(215)), w-S(36), "Offsets", "НЕ ПРОВЕРЕНЫ", ORANGE_LIGHT);
        separator(add(p, S(18), S(249)), w-S(36));
        text(d, f.small, add(p, S(18), S(263)), DIM,
             state.driver_state == DriverState::NotImplemented ? "Проверка драйвера в этой версии отсутствует." : "Состояние предоставлено адаптером драйвера.");

        const ImVec2 q = add(p, 0, S(312));
        card(q, w, 176);
        card_header(q, w, "ИНТЕГРАЦИЯ", "СЛЕДУЮЩИЙ ШАГ");
        text(d, f.regular, add(q, S(18), S(64)), WHITE, "Нужен исходный код backend");
        text(d, f.small, add(q, S(18), S(90)), MUTED, "Интерфейс драйвера, версия игры, цепочки");
        text(d, f.small, add(q, S(18), S(111)), MUTED, "объектов и способ получения матрицы камеры.");
        if (state.profile_status == ProfileStatus::Error && !state.profile_error.empty()) {
            ImGui::SetCursorScreenPos(add(q, S(18), S(140)));
            ImGui::InvisibleButton("##profile-error", ImVec2(w - S(36), S(24)), ImGuiButtonFlags_EnableNav);
            text(d, f.small, add(q, S(18), S(146)), ORANGE_LIGHT, "Ошибка профиля: наведите для деталей");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", state.profile_error.c_str());
        } else {
            text(d, f.small, add(q, S(18), S(146)), DIM, "В профиле: только настройки интерфейса.");
        }
    }

    void debug_right(ImVec2 p, float w) {
        card(p, w, 576);
        char badge[64];
        std::snprintf(badge, sizeof(badge), "%zu ENTRIES / UNVERIFIED", offset_count);
        card_header(p, w, "OFFSETS", badge);
        const float pad = S(18), inner = w - 2*pad;
        ImGui::SetCursorScreenPos(add(p, pad, S(62)));
        ImGui::SetNextItemWidth(inner);
        ImGui::PushFont(f.small);
        ImGui::InputTextWithHint("##offset-filter", "Поиск по группе или имени...", state.offset_filter, sizeof(state.offset_filter));
        ImGui::PopFont();
        auto* d = ImGui::GetWindowDrawList();
        text(d, f.small, add(p, pad, S(102)), DIM, "Значения из сообщения, без проверки версии.");
        ImGui::SetCursorScreenPos(add(p, pad, S(128)));
        ImGui::PushFont(f.small);
        const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
        if (ImGui::BeginTable("##offset-table", 3, flags, ImVec2(inner, S(377)))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("ГРУППА", ImGuiTableColumnFlags_WidthFixed, S(110));
            ImGui::TableSetupColumn("ПОЛЕ", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ЗНАЧЕНИЕ", ImGuiTableColumnFlags_WidthFixed, S(118));
            ImGui::TableHeadersRow();
            const std::string filter = lowercase(state.offset_filter);
            for (const auto& entry : offsets) {
                const std::string all = lowercase(std::string(entry.group) + " " + entry.name + " " + entry.value);
                if (!filter.empty() && all.find(filter) == std::string::npos) continue;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(.55f,.56f,.55f,1), "%s", entry.group);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.group);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(entry.name);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.name);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(ImVec4(1.0f,.55f,.31f,1), "%s", entry.value);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.value);
            }
            ImGui::EndTable();
        }
        ImGui::PopFont();
        if (button("##copy-offsets", add(p, pad, S(523)), V(156, 32), "КОПИРОВАТЬ ВСЕ")) {
            std::ostringstream output;
            for (const auto& entry : offsets) output << entry.group << " :: " << entry.name << " = " << entry.value << '\n';
            ImGui::SetClipboardText(output.str().c_str());
        }
        right(d, f.tiny, p.x + w - pad, p.y + S(533), DIM, "ТОЛЬКО СПРАВОЧНИК");
    }

    void column(const char* id, ImVec2 p, float width, float height, bool left_column, bool reset_scroll) {
        ImGui::SetCursorScreenPos(p);
        if (reset_scroll) ImGui::SetNextWindowScroll(ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        const bool visible = ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleVar();
        if (visible) {
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float actual_width = ImGui::GetContentRegionAvail().x;
            float used = 576.0f;
            switch (state.page) {
            case Page::Aimbot: if (left_column) aim_left(origin, actual_width); else aim_right(origin, actual_width); break;
            case Page::Visuals: if (left_column) visual_left(origin, actual_width); else visual_right(origin, actual_width); break;
            case Page::Resolver: if (left_column) resolver_left(origin, actual_width); else resolver_right(origin, actual_width); used = left_column ? 505.0f : 376.0f; break;
            case Page::Misc: if (left_column) misc_left(origin, actual_width); else misc_right(origin, actual_width); used = left_column ? 412.0f : 344.0f; break;
            case Page::Debug: if (left_column) debug_left(origin, actual_width); else debug_right(origin, actual_width); used = left_column ? 488.0f : 576.0f; break;
            }
            // A real item establishes the scrollable extent for smaller windows.
            ImGui::SetCursorScreenPos(add(origin, 0, S(used)));
            ImGui::Dummy(ImVec2(S(1), S(1)));
        }
        ImGui::EndChild();
    }

    void render() {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(display);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::Begin("##kingware-root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar(2);
        auto* d = ImGui::GetWindowDrawList();
        d->AddRectFilled(ImVec2(0,0), display, BG);
        sidebar(display.y);
        chrome(display.x, display.y);
        page_title(display.x);
        const float x = S(249), y = S(163), gap = S(14);
        const float available_w = std::max(S(200), display.x - x - S(31));
        // The offsets table needs a wider right column than the settings pages.
        const float split = state.page == Page::Debug ? .46f : .55f;
        const float left_w = (available_w - gap) * split;
        const float right_w = available_w - gap - left_w;
        const float content_h = std::max(S(50), display.y - y - S(46));
        const bool reset_scroll = state.page != previous_page;
        column("##left-column", ImVec2(x,y), left_w, content_h, true, reset_scroll);
        column("##right-column", ImVec2(x + left_w + gap,y), right_w, content_h, false, reset_scroll);
        previous_page = state.page;
        ImGui::End();
    }
};

} // namespace

void Interface::configure(float dpi_scale) {
    scale_ = std::clamp(dpi_scale, 0.75f, 3.0f);
    previous_page_ = static_cast<Page>(-1); // Reset scroll positions after a DPI rebuild.
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const ImWchar* cyrillic = io.Fonts->GetGlyphRangesCyrillic();
    std::vector<std::string> regular, bold, mono;
#ifdef _WIN32
    if (const char* windir = std::getenv("WINDIR")) {
        regular.emplace_back(std::string(windir) + "\\Fonts\\segoeui.ttf");
        bold.emplace_back(std::string(windir) + "\\Fonts\\segoeuib.ttf");
        mono.emplace_back(std::string(windir) + "\\Fonts\\consola.ttf");
    }
    regular.emplace_back("C:\\Windows\\Fonts\\segoeui.ttf");
    bold.emplace_back("C:\\Windows\\Fonts\\segoeuib.ttf");
    mono.emplace_back("C:\\Windows\\Fonts\\consola.ttf");
#else
    regular.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    bold.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    mono.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
#endif
    fonts_.regular = load_font(regular, 14.0f * scale_, cyrillic);
    fonts_.small = load_font(regular, 12.0f * scale_, cyrillic);
    fonts_.tiny = load_font(bold, 10.0f * scale_, cyrillic);
    fonts_.bold = load_font(bold, 14.0f * scale_, cyrillic);
    fonts_.heading = load_font(bold, 25.0f * scale_, cyrillic);
    fonts_.brand = load_font(bold, 16.0f * scale_, cyrillic);
    fonts_.mono = load_font(mono, 12.0f * scale_, cyrillic);
    io.FontDefault = fonts_.regular;

    // Rebuild from defaults on every DPI change rather than compounding scale.
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle{};
    ImGui::StyleColorsDark();
    style.WindowRounding = 5.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 5.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(9, 6);
    style.ItemSpacing = ImVec2(8, 7);
    style.ScrollbarSize = 8.0f;
    style.GrabMinSize = 10.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(.075f,.079f,.079f,1);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(.09f,.095f,.095f,1);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(.115f,.12f,.12f,1);
    style.Colors[ImGuiCol_Border] = ImVec4(.21f,.22f,.22f,1);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(.14f,.145f,.145f,1);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(.19f,.19f,.19f,1);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(.22f,.20f,.18f,1);
    style.Colors[ImGuiCol_Text] = ImVec4(.90f,.90f,.89f,1);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(.38f,.39f,.39f,1);
    style.Colors[ImGuiCol_Button] = ImVec4(.20f,.20f,.20f,1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.38f,.23f,.16f,1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(.85f,.38f,.17f,1);
    style.Colors[ImGuiCol_Header] = ImVec4(.26f,.17f,.13f,1);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(.34f,.21f,.15f,1);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(.45f,.25f,.16f,1);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1,.47f,.22f,1);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1,.47f,.22f,1);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1,.59f,.35f,1);
    style.Colors[ImGuiCol_Separator] = ImVec4(.18f,.19f,.19f,1);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(.075f,.08f,.08f,1);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(.27f,.27f,.27f,1);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(.39f,.27f,.21f,1);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(.75f,.37f,.20f,1);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(.14f,.15f,.15f,1);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0,0,0,0);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1,1,1,.025f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(.17f,.18f,.18f,1);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(1,.47f,.22f,1);
    style.ScaleAllSizes(scale_);
}

void Interface::render(AppState& state) {
    Renderer{scale_, fonts_, state, preview_cursor_, previous_page_}.render();
}

} // namespace kingware
