#pragma once
#include <windows.h>
#include <string>

struct Settings {
    // General
    int maxHistorySize = 1000;
    bool ignoreDuplicates = true;
    bool saveHistory = true;
    bool purgeBitmapsOnExit = false;
    bool runAtStartup = false;

    // Popup
    bool useCaretPosition = true;   // false = use mouse position
    bool showPreviewTooltips = true;
    int maxDisplayLength = 60;
    bool showAccelerators = true;
    int trayClickAction = 1;        // 0 = popup history, 1 = search dialog

    // Hotkey for popup (modifiers | vk)
    UINT hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    UINT hotkeyVK = 'V';

    // Hotkey for search
    UINT searchHotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    UINT searchHotkeyVK = 'S';

    // Hotkey for notes
    UINT notesHotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    UINT notesHotkeyVK = 'N';

    // Paths
    std::wstring iniPath;
    std::wstring historyPath;
    std::wstring notesPath;

    void Load();
    void Save() const;
    void InitPaths();

    // Autostart management
    void SetAutostart(bool enable) const;
    bool IsAutostartEnabled() const;
};
