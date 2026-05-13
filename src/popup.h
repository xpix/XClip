#pragma once
#include <windows.h>
#include "history.h"
#include "settings.h"

class PopupMenu {
public:
    // Show the clipboard history popup at the appropriate position
    // Returns the index of the selected entry, or -1 if cancelled
    int Show(HWND hwnd, const History& history, const Settings& settings);

    // Get popup position (caret or mouse)
    static POINT GetPopupPosition(bool useCaretPosition);

private:
    void BuildMenu(HMENU hMenu, const History& history, const Settings& settings);
};

// Simulate Ctrl+V to paste
void SimulatePaste();
