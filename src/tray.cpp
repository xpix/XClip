#include "tray.h"
#include "../resources/resource.h"
#include <algorithm>

bool TrayIcon::Create(HWND hwnd, HICON hIcon, const wchar_t* tooltip) {
    hwnd_ = hwnd;
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = hIcon;
    if (tooltip) {
        wcsncpy_s(nid_.szTip, tooltip, _TRUNCATE);
    }

    // Delete any stale icon from a previous crashed instance
    Shell_NotifyIconW(NIM_DELETE, &nid_);

    // Retry a few times — explorer may not be ready at startup
    for (int i = 0; i < 5; i++) {
        if (Shell_NotifyIconW(NIM_ADD, &nid_)) {
            created_ = true;
            break;
        }
        Sleep(500);
    }

    if (created_) {
        nid_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid_);
    }

    return created_;
}

void TrayIcon::Destroy() {
    if (created_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
}

void TrayIcon::SetTooltip(const std::wstring& text) {
    if (!created_) return;
    wcsncpy_s(nid_.szTip, text.c_str(), _TRUNCATE);
    nid_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::SetIcon(HICON hIcon) {
    if (!created_) return;
    nid_.hIcon = hIcon;
    nid_.uFlags = NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::ShowBalloon(const wchar_t* title, const wchar_t* text, DWORD flags) {
    if (!created_) return;
    nid_.uFlags = NIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(nid_.szInfo, text, _TRUNCATE);
    nid_.dwInfoFlags = flags;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}
