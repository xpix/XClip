#pragma once
#include <windows.h>
#include "clipboard.h"
#include "history.h"
#include "tray.h"
#include "hotkey.h"
#include "popup.h"
#include "settings.h"
#include "notes.h"

class App {
public:
    static App& Instance();

    bool Init(HINSTANCE hInstance);
    void Shutdown();
    int Run();

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND GetHwnd() const { return hwnd_; }
    Settings& GetSettings() { return settings_; }
    History& GetHistory() { return history_; }

private:
    App() = default;
    ~App() = default;

    bool CreateHiddenWindow();
    void RegisterHotkeys();
    void UnregisterHotkeys();
    void OnClipboardUpdate();
    void OnHotkey(int id);
    void OnTrayIcon(LPARAM lParam);
    void ShowTrayContextMenu();
    void PasteEntry(int index);
    void UpdateTooltip();
    void ShowAbout();
    void ShowNotes();

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    HICON hIcon_ = nullptr;

    Settings settings_;
    ClipboardMonitor clipboard_;
    History history_;
    TrayIcon tray_;
    HotkeyManager hotkeys_;
    PopupMenu popup_;
    NotesManager notes_;

    bool inPaste_ = false; // Flag to avoid capturing our own paste
    bool shutdownDone_ = false;
    bool dblClickHandled_ = false; // Suppress WM_LBUTTONUP after double-click
};
