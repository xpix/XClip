#pragma once
#include <windows.h>
#include <string>

struct Settings {
    // General
    int maxHistorySize = 25;
    bool ignoreDuplicates = true;
    bool saveHistory = true;
    bool purgeBitmapsOnExit = false;
    bool runAtStartup = false;

    // Popup
    bool useCaretPosition = true;   // false = use mouse position
    bool showPreviewTooltips = true;
    int maxDisplayLength = 60;
    bool showAccelerators = true;

    // Hotkey for popup (modifiers | vk)
    UINT hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    UINT hotkeyVK = 'V';

    // Paths
    std::wstring iniPath;
    std::wstring historyPath;

    void Load();
    void Save() const;
    void InitPaths();

    // Autostart management
    void SetAutostart(bool enable) const;
    bool IsAutostartEnabled() const;
};
