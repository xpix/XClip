#pragma once
#include <windows.h>
#include "history.h"
#include "settings.h"

class PopupMenu {
public:
    // Show the clipboard history popup at the appropriate position\n    // Returns the index of the selected entry, or -1 if cancelled
    int Show(HWND hwnd, const History& history, const Settings& settings);

    // Get popup position (caret or mouse)
    static POINT GetPopupPosition(bool useCaretPosition);

    // Called from WndProc on WM_MENUSELECT to update preview tooltip
    void OnMenuSelect(HWND hwnd, WPARAM wParam, LPARAM lParam);

    // Called from WndProc on WM_UNINITMENUPOPUP to hide tooltip
    void OnMenuClose();

private:
    void BuildMenu(HMENU hMenu, const History& history, const Settings& settings);
    void CreateTooltipWindow(HWND hwnd);
    void CreatePreviewWindow(HWND hwnd);
    void ShowTooltipForItem(int index);
    void ShowPreviewForBitmap(int index);
    void HidePreview();

    HWND hwndTooltip_ = nullptr;
    HWND hwndPreview_ = nullptr;
    HBITMAP hPreviewBitmap_ = nullptr;
    const History* currentHistory_ = nullptr;
    bool showTooltips_ = true;
};

// Simulate Ctrl+V to paste
void SimulatePaste();
