#include "tray.h"
#include "../resources/resource.h"
#include <algorithm>

bool TrayIcon::Create(HWND hwnd, HICON hIcon, const wchar_t* tooltip) {
    hwnd_ = hwnd;
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = hIcon;
    if (tooltip) {
        wcsncpy_s(nid_.szTip, tooltip, _TRUNCATE);
    }

    created_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;

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
