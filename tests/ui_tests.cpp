#include "ui.hpp"
#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>
#include <iostream>

namespace {
int failures = 0;
std::string clipboard;
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void frame(kingware::Interface& ui, kingware::AppState& state) {
    ImGui::NewFrame();
    ui.render(state);
    ImGui::Render();
    const ImDrawData* data = ImGui::GetDrawData();
    check(data && data->Valid && data->CmdListsCount > 0 && data->TotalVtxCount > 100, "render produces draw data");
}
void click(kingware::Interface& ui, kingware::AppState& state, float x, float y) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    frame(ui, state);
    io.AddMouseButtonEvent(0, true);
    frame(ui, state);
    io.AddMouseButtonEvent(0, false);
    frame(ui, state);
}
}

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1320, 820);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetPlatformIO().Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) { clipboard = text; };
    kingware::Interface ui;
    ui.configure(1.0f);
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    check(pixels && width > 0 && height > 0, "font atlas builds");
    kingware::AppState state;
    frame(ui, state);
    frame(ui, state);

    check(state.settings.visual_health, "default health setting enabled");
    click(ui, state, 350, 340);
    check(!state.settings.visual_health, "visual toggle responds to click");
    click(ui, state, 700, 299);
    frame(ui, state);
    frame(ui, state);
    check(ImGui::GetCurrentContext()->OpenPopupStack.Size > 0, "box style dropdown opens");
    if (ImGui::GetCurrentContext()->OpenPopupStack.Size > 0) {
        ImGuiWindow* popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        if (popup) click(ui, state, popup->Pos.x + 30.0f, popup->Pos.y + popup->Size.y - 10.0f);
        check(state.settings.visual_box == 2, "box style dropdown changes setting");
    }
    click(ui, state, 700, 574);
    check(state.settings.visual_distance_max > 500.0f, "visual slider responds to click");
    click(ui, state, 1220, 187);
    check(!state.demo_visible, "demo visibility button works");
    click(ui, state, 90, 153);
    check(state.page == kingware::Page::Aimbot, "sidebar changes page");
    click(ui, state, 350, 235);
    check(state.settings.aim_enabled, "aim preview toggle responds to click");
    click(ui, state, 1160, 32);
    check(state.request_save, "save button requests persistence");

    for (auto page : {kingware::Page::Aimbot, kingware::Page::Visuals, kingware::Page::Resolver,
                      kingware::Page::Misc, kingware::Page::Debug}) {
        state.page = page;
        frame(ui, state);
        frame(ui, state);
    }
    click(ui, state, 800, 236);
    io.AddInputCharactersUTF8("BasePlayer");
    frame(ui, state);
    check(std::strcmp(state.offset_filter, "BasePlayer") == 0, "offset search accepts input");
    click(ui, state, 800, 702);
    check(clipboard.find("Global :: BaseNetworkable = 0x2FF92B0 + 0x12F + 0xE0") != std::string::npos &&
          clipboard.find("ItemDefinition :: itemType = 0x4C") != std::string::npos,
          "copy offsets includes the entire unfiltered list");

    io.DisplaySize = ImVec2(1060, 680);
    state.page = kingware::Page::Visuals;
    frame(ui, state);
    frame(ui, state);
    ImGuiWindow* left_column = nullptr;
    for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
        if (std::strstr(window->Name, "##left-column")) left_column = window;
    }
    check(left_column && left_column->ScrollMax.y > 0, "small window makes settings scrollable");
    io.AddMousePosEvent(400, 400);
    io.AddMouseWheelEvent(0, -4);
    frame(ui, state);
    frame(ui, state);
    check(left_column && left_column->Scroll.y > 0, "mouse wheel scrolls a settings column");
    state.page = kingware::Page::Debug;
    frame(ui, state);
    check(left_column && left_column->Scroll.y == 0, "changing tabs resets the scroll position");
    for (auto page : {kingware::Page::Aimbot, kingware::Page::Visuals, kingware::Page::Resolver,
                      kingware::Page::Misc, kingware::Page::Debug}) {
        state.page = page;
        frame(ui, state);
    }
    ui.configure(1.5f);
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.DisplaySize = ImVec2(1980, 1230);
    state.page = kingware::Page::Visuals;
    frame(ui, state);
    ui.configure(1.0f);
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    check(ImGui::GetStyle().FramePadding.x == 9.0f, "DPI reconfiguration does not compound style sizes");
    io.DisplaySize = ImVec2(1320, 820);
    state.page = kingware::Page::Misc;
    frame(ui, state);
    frame(ui, state);
    state.settings.aim_smooth = 21.0f;
    click(ui, state, 450, 523);
    check(ImGui::GetCurrentContext()->OpenPopupStack.Size > 0, "reset asks for confirmation");
    if (ImGui::GetCurrentContext()->OpenPopupStack.Size > 0) {
        ImGuiWindow* popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        check(popup != nullptr, "confirmation dialog is visible");
        if (popup) {
            const float bottom_button_y = popup->Pos.y + popup->Size.y - ImGui::GetStyle().WindowPadding.y - 15.0f;
            const float cancel_x = popup->Pos.x + ImGui::GetStyle().WindowPadding.x + 110.0f + ImGui::GetStyle().ItemSpacing.x + 55.0f;
            click(ui, state, cancel_x, bottom_button_y);
            check(state.settings.aim_smooth == 21.0f, "cancel keeps settings intact");
        }
    }
    click(ui, state, 450, 523);
    if (ImGui::GetCurrentContext()->OpenPopupStack.Size > 0) {
        ImGuiWindow* popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        if (popup) {
            const float confirm_x = popup->Pos.x + ImGui::GetStyle().WindowPadding.x + 55.0f;
            const float confirm_y = popup->Pos.y + popup->Size.y - ImGui::GetStyle().WindowPadding.y - 15.0f;
            click(ui, state, confirm_x, confirm_y);
            check(state.settings.aim_smooth == 8.0f && state.settings.visual_health, "confirm resets settings to defaults");
        }
    } else {
        check(false, "confirmation dialog opens a second time");
    }
    click(ui, state, 1255, 32);
    check(state.request_minimize, "minimize button requests native minimization");
    click(ui, state, 1288, 32);
    check(state.request_close, "close button requests native closure");
    ImGui::DestroyContext();
    if (failures) { std::cerr << failures << " UI test(s) failed\n"; return 1; }
    std::cout << "All ImGui UI smoke tests passed.\n";
    return 0;
}
