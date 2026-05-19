cmake_minimum_required(VERSION 3.16)
project(XClip VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(XClip WIN32
    src/main.cpp
    src/app.cpp
    src/clipboard.cpp
    src/history.cpp
    src/tray.cpp
    src/popup.cpp
    src/hotkey.cpp
    src/settings.cpp
    src/config_dialog.cpp
    src/search_dialog.cpp
    src/notes.cpp
    src/utils.cpp
    resources/xclip.rc
)

target_include_directories(XClip PRIVATE src)
target_compile_definitions(XClip PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(XClip PRIVATE comctl32 gdi32 user32 shell32 ole32 shlwapi gdiplus)

# Enable manifest for visual styles
if(MSVC)
    target_compile_options(XClip PRIVATE /W4 /utf-8)
    set_target_properties(XClip PROPERTIES
        LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_SOURCE_DIR}/resources/xclip.manifest"
    )
endif()
