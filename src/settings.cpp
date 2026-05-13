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
}

void Settings::Load() {
    InitPaths();

    maxHistorySize = ReadInt(iniPath, L"General", L"MaxHistorySize", 25);
    if (maxHistorySize < 1) maxHistorySize = 1;
    if (maxHistorySize > 500) maxHistorySize = 500;

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

    hotkeyModifiers = (UINT)ReadInt(iniPath, L"Hotkey", L"Modifiers", MOD_CONTROL | MOD_SHIFT);
    hotkeyVK = (UINT)ReadInt(iniPath, L"Hotkey", L"VK", 'V');
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

    WriteInt(iniPath, L"Hotkey", L"Modifiers", (int)hotkeyModifiers);
    WriteInt(iniPath, L"Hotkey", L"VK", (int)hotkeyVK);
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
