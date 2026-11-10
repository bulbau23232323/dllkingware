cmake_minimum_required(VERSION 3.24)
project(Kingware VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pin both the commit and archive hash so that an upstream update cannot change
# the build. A source release can bundle ImGui here for fully offline builds.
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_IMGUI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp")
    set(FETCHCONTENT_SOURCE_DIR_IMGUI "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
endif()
include(FetchContent)
FetchContent_Declare(imgui
    URL https://codeload.github.com/ocornut/imgui/tar.gz/f5befd2d29e66809cd1110a152e375a7f1981f06
    URL_HASH SHA256=85f4ce357df05bcc331b587f01976f47fb55f19fadf477a7907289686bc3f4c8
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

add_library(kingware_core STATIC src/settings.cpp src/aim.cpp)
target_include_directories(kingware_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(kingware_ui STATIC src/ui.cpp)
target_include_directories(kingware_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kingware_ui PUBLIC kingware_core imgui)

if(MSVC)
    target_compile_options(kingware_core PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(kingware_ui PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(kingware_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(kingware_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_sources(imgui PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )
    target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui PUBLIC d3d11 dxgi dwmapi user32 gdi32 imm32)
    if(MSVC)
        target_link_libraries(imgui PUBLIC d3dcompiler)
    else()
        target_link_libraries(imgui PUBLIC d3dcompiler_47)
    endif()

    add_executable(Kingware WIN32 src/main_win32.cpp)
    target_link_libraries(Kingware PRIVATE kingware_ui)
    target_compile_definitions(Kingware PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(Kingware PRIVATE /W4 /permissive- /utf-8)
    else()
        # Some Clang/MinGW drivers ignore -mwindows when cross-compiling.
        target_link_options(Kingware PRIVATE -Wl,--subsystem,windows)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(kingware_core_tests tests/core_tests.cpp)
    target_link_libraries(kingware_core_tests PRIVATE kingware_core)
    add_test(NAME kingware_core_tests COMMAND kingware_core_tests)

    add_executable(kingware_ui_tests tests/ui_tests.cpp)
    target_link_libraries(kingware_ui_tests PRIVATE kingware_ui)
    add_test(NAME kingware_ui_tests COMMAND kingware_ui_tests)
endif()
