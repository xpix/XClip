#include "settings.h"
#include "utils.h"

static int ReadInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int def) {
    return GetPrivateProfileIntW(section, key, def, path.c_str());
}

static void WriteInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int value) {
    wchar_t buf[32];
    wsprintfW(buf, L"%d", value);
    WritePrivateProfileStringW(section, key, buf, path.c_str());
}

void Settings::InitPaths() {
    std::wstring appDir = Utils::GetAppDataPath();
    iniPath = appDir + L"\\xclip.ini";
    historyPath = appDir + L"\\history.dat";
    notesPath = appDir + L"\\notes.dat";
}

void Settings::Load() {
    InitPaths();

    maxHistorySize = ReadInt(iniPath, L"General", L"MaxHistorySize", 1000);
    if (maxHistorySize < 1) maxHistorySize = 1;
    if (maxHistorySize > 5000) maxHistorySize = 5000;

    ignoreDuplicates = ReadInt(iniPath, L"General", L"IgnoreDuplicates", 1) != 0;
    saveHistory = ReadInt(iniPath, L"General", L"SaveHistory", 1) != 0;
    purgeBitmapsOnExit = ReadInt(iniPath, L"General", L"PurgeBitmaps", 0) != 0;
    runAtStartup = IsAutostartEnabled();

    useCaretPosition = ReadInt(iniPath, L"Popup", L"UseCaretPosition", 1) != 0;
    showPreviewTooltips = ReadInt(iniPath, L"Popup", L"ShowPreviewTooltips", 1) != 0;
    maxDisplayLength = ReadInt(iniPath, L"Popup", L"MaxDisplayLength", 60);
    if (maxDisplayLength < 10) maxDisplayLength = 10;
    if (maxDisplayLength > 200) maxDisplayLength = 200;
    showAccelerators = ReadInt(iniPath, L"Popup", L"ShowAccelerators", 1) != 0;
    trayClickAction = ReadInt(iniPath, L"Popup", L"TrayClickAction", 1);
    if (trayClickAction < 0 || trayClickAction > 1) trayClickAction = 1;

    hotkeyModifiers = (UINT)ReadInt(iniPath, L"Hotkey", L"Modifiers", MOD_CONTROL | MOD_SHIFT);
    hotkeyVK = (UINT)ReadInt(iniPath, L"Hotkey", L"VK", 'V');

    searchHotkeyModifiers = (UINT)ReadInt(iniPath, L"Hotkey", L"SearchModifiers", MOD_CONTROL | MOD_SHIFT);
    searchHotkeyVK = (UINT)ReadInt(iniPath, L"Hotkey", L"SearchVK", 'S');

    notesHotkeyModifiers = (UINT)ReadInt(iniPath, L"Hotkey", L"NotesModifiers", MOD_CONTROL | MOD_SHIFT);
    notesHotkeyVK = (UINT)ReadInt(iniPath, L"Hotkey", L"NotesVK", 'N');
}

void Settings::Save() const {
    WriteInt(iniPath, L"General", L"MaxHistorySize", maxHistorySize);
    WriteInt(iniPath, L"General", L"IgnoreDuplicates", ignoreDuplicates ? 1 : 0);
    WriteInt(iniPath, L"General", L"SaveHistory", saveHistory ? 1 : 0);
    WriteInt(iniPath, L"General", L"PurgeBitmaps", purgeBitmapsOnExit ? 1 : 0);

    WriteInt(iniPath, L"Popup", L"UseCaretPosition", useCaretPosition ? 1 : 0);
    WriteInt(iniPath, L"Popup", L"ShowPreviewTooltips", showPreviewTooltips ? 1 : 0);
    WriteInt(iniPath, L"Popup", L"MaxDisplayLength", maxDisplayLength);
    WriteInt(iniPath, L"Popup", L"ShowAccelerators", showAccelerators ? 1 : 0);
    WriteInt(iniPath, L"Popup", L"TrayClickAction", trayClickAction);

    WriteInt(iniPath, L"Hotkey", L"Modifiers", (int)hotkeyModifiers);
    WriteInt(iniPath, L"Hotkey", L"VK", (int)hotkeyVK);

    WriteInt(iniPath, L"Hotkey", L"SearchModifiers", (int)searchHotkeyModifiers);
    WriteInt(iniPath, L"Hotkey", L"SearchVK", (int)searchHotkeyVK);

    WriteInt(iniPath, L"Hotkey", L"NotesModifiers", (int)notesHotkeyModifiers);
    WriteInt(iniPath, L"Hotkey", L"NotesVK", (int)notesHotkeyVK);
}

static const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kAppName = L"XClip";

void Settings::SetAutostart(bool enable) const {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            std::wstring exePath = L"\"" + Utils::GetExePath() + L"\"";
            RegSetValueExW(hKey, kAppName, 0, REG_SZ,
                (const BYTE*)exePath.c_str(),
                (DWORD)((exePath.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, kAppName);
        }
        RegCloseKey(hKey);
    }
}

bool Settings::IsAutostartEnabled() const {
    HKEY hKey;
    bool enabled = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        enabled = (RegQueryValueExW(hKey, kAppName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
        RegCloseKey(hKey);
    }
    return enabled;
}
