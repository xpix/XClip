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
    hotkeys_.Register(hwnd_, HOTKEY_SEARCH,
        settings_.searchHotkeyModifiers, settings_.searchHotkeyVK);
    hotkeys_.Register(hwnd_, HOTKEY_NOTES,
        settings_.notesHotkeyModifiers, settings_.notesHotkeyVK);
}

void App::UnregisterHotkeys() {
    hotkeys_.UnregisterAll(hwnd_);
}

bool App::Init(HINSTANCE hInstance) {
    hInstance_ = hInstance;

    auto dbg = [](const wchar_t* msg) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring logPath(exePath);
        auto pos = logPath.find_last_of(L'\\');
        if (pos != std::wstring::npos) logPath = logPath.substr(0, pos);
        logPath += L"\\debug.log";
        HANDLE hFile = CreateFileW(logPath.c_str(),
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
    // Use a well-known system icon for the tray (clipboard icon from shell32)
    hIcon_ = (HICON)LoadImageW(GetModuleHandleW(L"shell32.dll"),
        MAKEINTRESOURCE(260), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (!hIcon_) {
        dbg(L"  Init: Shell32 icon 260 failed, trying IDI_APPLICATION...");
        hIcon_ = (HICON)LoadImageW(nullptr, IDI_APPLICATION, IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    }
    {
        wchar_t buf[128];
        wsprintfW(buf, L"  Init: hIcon=%p, SM_CXSMICON=%d", (void*)hIcon_, GetSystemMetrics(SM_CXSMICON));
        dbg(buf);
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

    // Show startup balloon so user can find the tray icon
    tray_.ShowBalloon(L"XClip", L"Clipboard Manager is running.\nCtrl+Shift+V = History | Ctrl+Shift+S = Search");

    dbg(L"  Init: Register hotkeys...");
    if (!hotkeys_.Register(hwnd_, HOTKEY_POPUP, settings_.hotkeyModifiers, settings_.hotkeyVK)) {
        dbg(L"  Init: Popup hotkey registration failed (non-fatal)");
    } else {
        dbg(L"  Init: Popup hotkey OK");
    }
    if (!hotkeys_.Register(hwnd_, HOTKEY_SEARCH, settings_.searchHotkeyModifiers, settings_.searchHotkeyVK)) {
        dbg(L"  Init: Search hotkey registration failed (non-fatal)");
    } else {
        dbg(L"  Init: Search hotkey OK");
    }
    if (!hotkeys_.Register(hwnd_, HOTKEY_NOTES, settings_.notesHotkeyModifiers, settings_.notesHotkeyVK)) {
        dbg(L"  Init: Notes hotkey registration failed (non-fatal)");
    } else {
        dbg(L"  Init: Notes hotkey OK");
    }

    // Load history from disk
    history_.SetMaxSize(settings_.maxHistorySize);
    if (settings_.saveHistory) {
        history_.LoadFromFile(settings_.historyPath);
    }

    // Load notes
    notes_.LoadFromFile(settings_.notesPath);

    // Register in Windows autostart
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, L"XClip", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(exePath),
                (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }
    }

    UpdateTooltip();

    return true;
}

void App::Shutdown() {
    if (shutdownDone_) return;
    shutdownDone_ = true;

    // Save history
    if (settings_.saveHistory) {
        history_.SaveToFile(settings_.historyPath, settings_.purgeBitmapsOnExit);
    }

    // Save notes
    notes_.SaveToFile(settings_.notesPath);

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
        if (selected == -2) {
            // "More items" clicked — open search
            int idx = ShowSearchDialog(hwnd_, history_);
            if (idx >= 0) {
                if (hwndFg) SetForegroundWindow(hwndFg);
                PasteEntry(idx);
            }
        } else if (selected >= 0 && selected < history_.GetCount()) {
            // Restore foreground window
            if (hwndFg) SetForegroundWindow(hwndFg);
            PasteEntry(selected);
        }
    } else if (id == HOTKEY_SEARCH) {
        int idx = ShowSearchDialog(hwnd_, history_);
        if (idx >= 0) {
            PasteEntry(idx);
        }
    } else if (id == HOTKEY_NOTES) {
        ShowNotes();
    }
}

void App::PasteEntry(int index) {
    if (index < 0 || index >= history_.GetCount()) return;

    inPaste_ = true;
    clipboard_.SetIgnoreNext(true);

    // Set the clipboard content
    const ClipEntry& entry = history_.GetEntry(index);
    if (!entry.RestoreToClipboard(hwnd_)) {
        inPaste_ = false;
        clipboard_.SetIgnoreNext(false);
        return;
    }

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
        if (dblClickHandled_) {
            dblClickHandled_ = false;
        } else {
            ShowNotes();
        }
        break;
    case WM_LBUTTONDBLCLK:
        dblClickHandled_ = true;
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
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_NOTES, L"&Notes...");
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
            // Re-register hotkeys if any changed
            if (settings_.hotkeyModifiers != oldSettings.hotkeyModifiers ||
                settings_.hotkeyVK != oldSettings.hotkeyVK ||
                settings_.searchHotkeyModifiers != oldSettings.searchHotkeyModifiers ||
                settings_.searchHotkeyVK != oldSettings.searchHotkeyVK ||
                settings_.notesHotkeyModifiers != oldSettings.notesHotkeyModifiers ||
                settings_.notesHotkeyVK != oldSettings.notesHotkeyVK) {
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
    case IDM_TRAY_NOTES:
        ShowNotes();
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

void App::ShowNotes() {
    ShowNotesManager(hwnd_, notes_);
    // Auto-save after closing the dialog
    notes_.SaveToFile(settings_.notesPath);
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
        // Debounce: screenshots and some apps trigger multiple rapid updates.
        // Restart a short timer — only capture when it fires.
        KillTimer(hwnd, 102);
        SetTimer(hwnd, 102, 200, nullptr);
        return 0;

    case WM_HOTKEY:
        OnHotkey((int)wParam);
        return 0;

    case WM_TRAYICON:
        OnTrayIcon(lParam);
        return 0;

    case WM_MENUSELECT:
        popup_.OnMenuSelect(hwnd, wParam, lParam);
        return 0;

    case WM_UNINITMENUPOPUP:
        popup_.OnMenuClose();
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
        } else if (wParam == 102) {
            // Debounced clipboard capture
            KillTimer(hwnd, 102);
            OnClipboardUpdate();
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
