#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>

class TrayIcon {
public:
    bool Create(HWND hwnd, HICON hIcon, const wchar_t* tooltip = L"XClip");
    void Destroy();
    void SetTooltip(const std::wstring& text);
    void SetIcon(HICON hIcon);

    HWND GetHwnd() const { return hwnd_; }

private:
    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_ = {};
    bool created_ = false;
};
