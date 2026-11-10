#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "settings.hpp"
#include "ui.hpp"

#include <algorithm>
#include <string>

// Dear ImGui intentionally leaves this declaration out of its public header.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
ID3D11Device* device = nullptr;
ID3D11DeviceContext* device_context = nullptr;
IDXGISwapChain* swap_chain = nullptr;
ID3D11RenderTargetView* render_target = nullptr;
UINT pending_width = 0, pending_height = 0;
bool swap_chain_occluded = false;
float dpi_scale = 1.0f;
bool reload_fonts = false;

void cleanup_render_target() {
    if (render_target) { render_target->Release(); render_target = nullptr; }
}

bool create_render_target() {
    ID3D11Texture2D* back_buffer = nullptr;
    HRESULT hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr) || !back_buffer) return false;
    hr = device->CreateRenderTargetView(back_buffer, nullptr, &render_target);
    back_buffer->Release();
    return SUCCEEDED(hr) && render_target;
}

void cleanup_device() {
    cleanup_render_target();
    if (swap_chain) { swap_chain->Release(); swap_chain = nullptr; }
    if (device_context) { device_context->Release(); device_context = nullptr; }
    if (device) { device->Release(); device = nullptr; }
}

bool create_device(HWND window) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL selected{};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                                D3D11_SDK_VERSION, &desc, &swap_chain, &device,
                                                &selected, &device_context);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                            D3D11_SDK_VERSION, &desc, &swap_chain, &device,
                                            &selected, &device_context);
    }
    return SUCCEEDED(hr) && create_render_target();
}

LRESULT WINAPI window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam)) return true;
    switch (message) {
    case WM_NCCALCSIZE:
        // Keep a resizable native frame without reserving a Windows title bar.
        if (wparam) return 0;
        break;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            pending_width = static_cast<UINT>(LOWORD(lparam));
            pending_height = static_cast<UINT>(HIWORD(lparam));
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = static_cast<LONG>(1060 * dpi_scale);
        info->ptMinTrackSize.y = static_cast<LONG>(680 * dpi_scale);
        return 0;
    }
    case WM_DPICHANGED: {
        dpi_scale = std::clamp(static_cast<float>(HIWORD(wparam)) / 96.0f, .75f, 3.0f);
        const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        reload_fonts = true;
        return 0;
    }
    case WM_NCHITTEST: {
        RECT rect{};
        GetWindowRect(hwnd, &rect);
        const int x = GET_X_LPARAM(lparam) - rect.left;
        const int y = GET_Y_LPARAM(lparam) - rect.top;
        const int w = rect.right - rect.left, h = rect.bottom - rect.top;
        const int edge = std::max(5, static_cast<int>(6 * dpi_scale));
        if (y < edge && x < edge) return HTTOPLEFT;
        if (y < edge && x >= w - edge) return HTTOPRIGHT;
        if (y >= h - edge && x < edge) return HTBOTTOMLEFT;
        if (y >= h - edge && x >= w - edge) return HTBOTTOMRIGHT;
        if (y < edge) return HTTOP;
        if (y >= h - edge) return HTBOTTOM;
        if (x < edge) return HTLEFT;
        if (x >= w - edge) return HTRIGHT;
        // The right side contains real buttons and must remain client area.
        if (y < static_cast<int>(64 * dpi_scale) && x < w - static_cast<int>(320 * dpi_scale)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}
} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
    ImGui_ImplWin32_EnableDpiAwareness();
    const POINT origin{0, 0};
    dpi_scale = std::clamp(ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY)), .75f, 3.0f);

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_CLASSDC;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = L"KingwareExternalWindow";
    if (!RegisterClassExW(&window_class)) {
        MessageBoxW(nullptr, L"Не удалось зарегистрировать окно.", L"Kingware", MB_OK | MB_ICONERROR);
        return 1;
    }
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int width = std::min(static_cast<int>(1320 * dpi_scale), static_cast<int>(work.right - work.left));
    const int height = std::min(static_cast<int>(820 * dpi_scale), static_cast<int>(work.bottom - work.top));
    const int x = work.left + (work.right - work.left - width) / 2;
    const int y = work.top + (work.bottom - work.top - height) / 2;
    HWND hwnd = CreateWindowExW(0, window_class.lpszClassName, L"Kingware — External GUI",
                                WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU,
                                x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!hwnd || !create_device(hwnd)) {
        MessageBoxW(nullptr, L"Не удалось создать окно DirectX 11.", L"Kingware", MB_OK | MB_ICONERROR);
        cleanup_device();
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassW(window_class.lpszClassName, instance);
        return 1;
    }
    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    kingware::Interface gui;
    gui.configure(dpi_scale);
    reload_fonts = false;
    const bool platform_initialized = ImGui_ImplWin32_Init(hwnd);
    const bool renderer_initialized = platform_initialized && ImGui_ImplDX11_Init(device, device_context);
    if (!renderer_initialized) {
        MessageBoxW(hwnd, L"Не удалось запустить Dear ImGui.", L"Kingware", MB_OK | MB_ICONERROR);
        if (platform_initialized) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        cleanup_device();
        DestroyWindow(hwnd);
        UnregisterClassW(window_class.lpszClassName, instance);
        return 1;
    }

    kingware::AppState state;
    kingware::UnavailableMemoryReader driver; // Replace only after the actual driver contract is known.
    state.driver_state = driver.status();
    const auto settings_path = kingware::default_settings_path();
    std::string error;
    const auto load = kingware::load_settings(settings_path, state.settings, error);
    if (load == kingware::LoadResult::Loaded) state.profile_status = kingware::ProfileStatus::Loaded;
    if (load == kingware::LoadResult::LoadedWithWarnings || load == kingware::LoadResult::Error) {
        state.profile_status = kingware::ProfileStatus::Error;
        state.profile_error = error;
    }
    std::string observed = kingware::serialize_settings(state.settings);
    bool pending_save = false, done = false;
    double changed_at = 0.0, retry_at = 0.0;

    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        if (swap_chain_occluded && swap_chain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(30);
            continue;
        }
        swap_chain_occluded = false;
        if (pending_width && pending_height) {
            cleanup_render_target();
            const HRESULT resized = swap_chain->ResizeBuffers(0, pending_width, pending_height, DXGI_FORMAT_UNKNOWN, 0);
            pending_width = pending_height = 0;
            if (FAILED(resized) || !create_render_target()) {
                MessageBoxW(hwnd, L"Не удалось изменить размер буфера DirectX.", L"Kingware", MB_OK | MB_ICONERROR);
                break;
            }
        }
        if (reload_fonts) {
            ImGui_ImplDX11_InvalidateDeviceObjects();
            gui.configure(dpi_scale);
            ImGui_ImplDX11_CreateDeviceObjects();
            reload_fonts = false;
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        state.driver_state = driver.status();
        gui.render(state);

        const double now = ImGui::GetTime();
        const std::string current = kingware::serialize_settings(state.settings);
        if (current != observed) {
            observed = current;
            changed_at = now;
            retry_at = 0;
            pending_save = true;
            state.profile_status = kingware::ProfileStatus::Saving;
            state.profile_error.clear();
        }
        if (state.request_save || (pending_save && now - changed_at >= .6 && now >= retry_at)) {
            state.request_save = false;
            if (kingware::save_settings(settings_path, state.settings, error)) {
                pending_save = false;
                state.profile_status = kingware::ProfileStatus::Saved;
                state.profile_error.clear();
            } else {
                pending_save = true;
                retry_at = now + 5.0;
                state.profile_status = kingware::ProfileStatus::Error;
                state.profile_error = error;
            }
        }
        if (state.request_minimize) { state.request_minimize = false; ShowWindow(hwnd, SW_MINIMIZE); }
        if (state.request_close) { state.request_close = false; PostMessageW(hwnd, WM_CLOSE, 0, 0); }

        ImGui::Render();
        const float clear_color[4] = {0.059f, 0.063f, 0.067f, 1.0f};
        device_context->OMSetRenderTargets(1, &render_target, nullptr);
        device_context->ClearRenderTargetView(render_target, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        const HRESULT result = swap_chain->Present(1, 0);
        swap_chain_occluded = result == DXGI_STATUS_OCCLUDED;
    }
    if (pending_save && !kingware::save_settings(settings_path, state.settings, error)) {
        MessageBoxW(nullptr, L"Не удалось сохранить профиль настроек.", L"Kingware", MB_OK | MB_ICONWARNING);
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_device();
    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    UnregisterClassW(window_class.lpszClassName, instance);
    return 0;
}
