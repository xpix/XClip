#include "app.h"
#include "config_dialog.h"
#include "search_dialog.h"
#include "utils.h"
#include "../resources/resource.h"
#include <commctrl.h>

static const wchar_t* kWindowClass = L"XCLIP_MAIN";
static const wchar_t* kMutexName = L"XClip_SingleInstance_Mutex";
static UINT WM_TASKBAR_CREATED = 0;

App& App::Instance() {
    static App instance;
    return instance;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return App::Instance().HandleMessage(hwnd, msg, wParam, lParam);
}

bool App::CreateHiddenWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance_;
    wc.lpszClassName = kWindowClass;
    wc.hIcon = hIcon_;

    if (!RegisterClassExW(&wc)) return false;

    // Use a normal hidden window (not HWND_MESSAGE) so we receive
    // WM_CLIPBOARDUPDATE and system tray messages reliably
    hwnd_ = CreateWindowExW(0, kWindowClass, L"XClip",
        WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance_, nullptr);

    return hwnd_ != nullptr;
}

void App::RegisterHotkeys() {
    hotkeys_.Register(hwnd_, HOTKEY_POPUP,
        settings_.hotkeyModifiers, settings_.hotkeyVK);
}

void App::UnregisterHotkeys() {
    hotkeys_.UnregisterAll(hwnd_);
}

bool App::Init(HINSTANCE hInstance) {
    hInstance_ = hInstance;

    auto dbg = [](const wchar_t* msg) {
        HANDLE hFile = CreateFileW(L"C:\\Users\\D063239\\Desktop\\XClip\\debug.log",
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD w;
            WriteFile(hFile, msg, (DWORD)(wcslen(msg) * sizeof(wchar_t)), &w, nullptr);
            WriteFile(hFile, L"\r\n", 4, &w, nullptr);
            CloseHandle(hFile);
        }
    };

    dbg(L"  Init: TaskbarCreated msg...");
    WM_TASKBAR_CREATED = RegisterWindowMessageW(L"TaskbarCreated");

    dbg(L"  Init: CommonControls...");
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS | ICC_HOTKEY_CLASS };
    InitCommonControlsEx(&icc);

    dbg(L"  Init: Load settings...");
    settings_.Load();

    dbg(L"  Init: Load icon...");
    hIcon_ = (HICON)LoadImageW(hInstance, MAKEINTRESOURCE(IDI_XCLIP),
        IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!hIcon_) {
        dbg(L"  Init: Icon from resource failed, using fallback");
        hIcon_ = LoadIcon(nullptr, IDI_APPLICATION);
    }

    dbg(L"  Init: Create window...");
    if (!CreateHiddenWindow()) {
        wchar_t buf[128];
        wsprintfW(buf, L"  Init: CreateWindow FAILED, GetLastError=%d", GetLastError());
        dbg(buf);
        return false;
    }
    dbg(L"  Init: Window created OK");

    dbg(L"  Init: Start clipboard monitor...");
    if (!clipboard_.Start(hwnd_)) {
        wchar_t buf[128];
        wsprintfW(buf, L"  Init: Clipboard FAILED, GetLastError=%d", GetLastError());
        dbg(buf);
        return false;
    }
    dbg(L"  Init: Clipboard OK");

    dbg(L"  Init: Create tray icon...");
    if (!tray_.Create(hwnd_, hIcon_, L"XClip - Clipboard Manager")) {
        wchar_t buf[128];
        wsprintfW(buf, L"  Init: TrayIcon FAILED, GetLastError=%d", GetLastError());
        dbg(buf);
        return false;
    }
    dbg(L"  Init: Tray OK");

    dbg(L"  Init: Register hotkey...");
    if (!hotkeys_.Register(hwnd_, HOTKEY_POPUP, settings_.hotkeyModifiers, settings_.hotkeyVK)) {
        dbg(L"  Init: Hotkey registration failed (non-fatal)");
    } else {
        dbg(L"  Init: Hotkey OK");
    }

    // Load history from disk
    history_.SetMaxSize(settings_.maxHistorySize);
    if (settings_.saveHistory) {
        history_.LoadFromFile(settings_.historyPath);
    }

    UpdateTooltip();

    return true;
}

void App::Shutdown() {
    // Save history
    if (settings_.saveHistory) {
        history_.SaveToFile(settings_.historyPath, settings_.purgeBitmapsOnExit);
    }

    UnregisterHotkeys();
    clipboard_.Stop();
    tray_.Destroy();

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    UnregisterClassW(kWindowClass, hInstance_);
}

int App::Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

void App::OnClipboardUpdate() {
    if (inPaste_) return;
    if (clipboard_.ShouldIgnore()) {
        clipboard_.OnClipboardUpdate();
        return;
    }

    if (history_.CaptureFromClipboard(hwnd_, settings_.ignoreDuplicates)) {
        UpdateTooltip();
    }
}

void App::OnHotkey(int id) {
    if (id == HOTKEY_POPUP) {
        // Store the foreground window before showing popup
        HWND hwndFg = GetForegroundWindow();

        int selected = popup_.Show(hwnd_, history_, settings_);
        if (selected >= 0 && selected < history_.GetCount()) {
            // Restore foreground window
            if (hwndFg) SetForegroundWindow(hwndFg);
            PasteEntry(selected);
        }
    }
}

void App::PasteEntry(int index) {
    if (index < 0 || index >= history_.GetCount()) return;

    inPaste_ = true;
    clipboard_.SetIgnoreNext(true);

    // Set the clipboard content
    const ClipEntry& entry = history_.GetEntry(index);
    entry.RestoreToClipboard(hwnd_);

    // Small delay then simulate Ctrl+V
    // Use a timer to allow message processing
    SetTimer(hwnd_, 100, 50, nullptr);

    // Move selected entry to top (if not already)
    // We don't actually reorder — the clipboard update will re-add it
}

void App::OnTrayIcon(LPARAM lParam) {
    UINT msg = LOWORD(lParam);
    switch (msg) {
    case WM_RBUTTONUP:
        ShowTrayContextMenu();
        break;
    case WM_LBUTTONUP:
        // Left click: show popup history
        OnHotkey(HOTKEY_POPUP);
        break;
    case WM_LBUTTONDBLCLK:
        // Double-click: show settings
        ShowConfigDialog(hwnd_, settings_);
        break;
    }
}

void App::ShowTrayContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Add history items at the top
    int count = history_.GetCount();
    int maxItems = (std::min)(count, 10);
    for (int i = 0; i < maxItems; i++) {
        const ClipEntry& entry = history_.GetEntry(i);
        std::wstring display = entry.GetDisplayText(settings_.maxDisplayLength);
        UINT flags = MF_STRING;
        if (i == 0) flags |= MF_DEFAULT;
        AppendMenuW(hMenu, flags, IDM_CLIP_BASE + i, display.c_str());
    }

    if (count == 0) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(empty)");
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SEARCH, L"&Search...");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLEAR_CURRENT, L"Clear C&urrent");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLEAR_HISTORY, L"Clear &History");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SETTINGS, L"Se&ttings...");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_ABOUT, L"&About XClip");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"E&xit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);

    int cmd = TrackPopupMenu(hMenu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        pt.x, pt.y, 0, hwnd_, nullptr);

    DestroyMenu(hMenu);
    PostMessage(hwnd_, WM_NULL, 0, 0);

    // Handle commands
    if (cmd >= IDM_CLIP_BASE && cmd <= IDM_CLIP_MAX) {
        PasteEntry(cmd - IDM_CLIP_BASE);
        return;
    }

    switch (cmd) {
    case IDM_TRAY_SETTINGS: {
        Settings oldSettings = settings_;
        if (ShowConfigDialog(hwnd_, settings_)) {
            // Re-register hotkeys if changed
            if (settings_.hotkeyModifiers != oldSettings.hotkeyModifiers ||
                settings_.hotkeyVK != oldSettings.hotkeyVK) {
                UnregisterHotkeys();
                RegisterHotkeys();
            }
            history_.SetMaxSize(settings_.maxHistorySize);
        }
        break;
    }
    case IDM_TRAY_SEARCH: {
        int idx = ShowSearchDialog(hwnd_, history_);
        if (idx >= 0) {
            PasteEntry(idx);
        }
        break;
    }
    case IDM_TRAY_CLEAR_CURRENT:
        if (OpenClipboard(hwnd_)) {
            EmptyClipboard();
            CloseClipboard();
        }
        break;
    case IDM_TRAY_CLEAR_HISTORY:
        history_.Clear();
        UpdateTooltip();
        break;
    case IDM_TRAY_ABOUT:
        ShowAbout();
        break;
    case IDM_TRAY_EXIT:
        PostQuitMessage(0);
        break;
    }
}

void App::UpdateTooltip() {
    if (history_.GetCount() > 0) {
        std::wstring tip = L"XClip - " +
            Utils::Truncate(Utils::SingleLine(history_.GetEntry(0).text), 80);
        tray_.SetTooltip(tip);
    } else {
        tray_.SetTooltip(L"XClip - Clipboard Manager");
    }
}

void App::ShowAbout() {
    MessageBoxW(hwnd_,
        L"XClip Clipboard Manager v1.0\n\n"
        L"A lightweight clipboard history manager for Windows.\n"
        L"Inspired by ClipX by Francis Gastellu.\n\n"
        L"Hotkey: Ctrl+Shift+V (configurable)\n"
        L"Right-click tray icon for menu.\n"
        L"Double-click tray icon for settings.",
        L"About XClip",
        MB_OK | MB_ICONINFORMATION);
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLIPBOARDUPDATE:
        OnClipboardUpdate();
        return 0;

    case WM_HOTKEY:
        OnHotkey((int)wParam);
        return 0;

    case WM_TRAYICON:
        OnTrayIcon(lParam);
        return 0;

    case WM_TIMER:
        if (wParam == 100) {
            KillTimer(hwnd, 100);
            SimulatePaste();
            // Reset paste flag after a short delay
            SetTimer(hwnd, 101, 100, nullptr);
        } else if (wParam == 101) {
            KillTimer(hwnd, 101);
            inPaste_ = false;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_ENDSESSION:
        Shutdown();
        return 0;

    // Handle taskbar recreation (e.g. explorer.exe restart)
    default:
        if (WM_TASKBAR_CREATED && msg == WM_TASKBAR_CREATED) {
            tray_.Destroy();
            tray_.Create(hwnd_, hIcon_, L"XClip - Clipboard Manager");
            UpdateTooltip();
            return 0;
        }
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
