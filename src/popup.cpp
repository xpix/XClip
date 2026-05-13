#include "popup.h"
#include "utils.h"
#include "../resources/resource.h"
#include <algorithm>

POINT PopupMenu::GetPopupPosition(bool useCaretPosition) {
    POINT pt;

    if (useCaretPosition) {
        // Try to get the caret position in the foreground window
        HWND hwndFg = GetForegroundWindow();
        if (hwndFg) {
            DWORD tid = GetWindowThreadProcessId(hwndFg, nullptr);
            DWORD curTid = GetCurrentThreadId();
            if (AttachThreadInput(curTid, tid, TRUE)) {
                HWND hwndFocus = GetFocus();
                if (hwndFocus) {
                    POINT caretPt = {};
                    if (GetCaretPos(&caretPt)) {
                        ClientToScreen(hwndFocus, &caretPt);
                        // Offset below the caret
                        caretPt.y += 20;
                        AttachThreadInput(curTid, tid, FALSE);
                        return caretPt;
                    }
                }
                AttachThreadInput(curTid, tid, FALSE);
            }
        }
    }

    // Fallback to mouse position
    GetCursorPos(&pt);
    return pt;
}

void PopupMenu::BuildMenu(HMENU hMenu, const History& history, const Settings& settings) {
    int count = history.GetCount();
    if (count == 0) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(empty)");
        return;
    }

    int maxItems = (std::min)(count, 25); // Show at most 25 items in popup
    for (int i = 0; i < maxItems; i++) {
        const ClipEntry& entry = history.GetEntry(i);
        std::wstring display = entry.GetDisplayText(settings.maxDisplayLength);

        // Add accelerator number (1-9, 0 for 10th)
        if (settings.showAccelerators && i < 10) {
            wchar_t prefix[8];
            wsprintfW(prefix, L"&%d  ", (i + 1) % 10);
            display = prefix + display;
        }

        UINT flags = MF_STRING;
        if (i == 0) flags |= MF_DEFAULT; // Bold the most recent entry

        // For bitmap entries, we could add MF_OWNERDRAW but keep it simple for now
        AppendMenuW(hMenu, flags, IDM_CLIP_BASE + i, display.c_str());
    }

    if (count > maxItems) {
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        wchar_t buf[64];
        wsprintfW(buf, L"(%d more items...)", count - maxItems);
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, buf);
    }
}

int PopupMenu::Show(HWND hwnd, const History& history, const Settings& settings) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return -1;

    BuildMenu(hMenu, history, settings);

    POINT pt = GetPopupPosition(settings.useCaretPosition);

    // Ensure our window is foreground so the menu dismisses properly
    SetForegroundWindow(hwnd);

    int cmd = TrackPopupMenu(hMenu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON,
        pt.x, pt.y, 0, hwnd, nullptr);

    DestroyMenu(hMenu);

    // Required after TrackPopupMenu to dismiss properly
    PostMessage(hwnd, WM_NULL, 0, 0);

    if (cmd >= IDM_CLIP_BASE && cmd <= IDM_CLIP_MAX) {
        return cmd - IDM_CLIP_BASE;
    }

    return -1;
}

void SimulatePaste() {
    // Small delay to let the menu close and focus return
    // Release any modifier keys first
    INPUT inputs[4] = {};

    // Key down: Ctrl
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    // Key down: V
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    // Key up: V
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // Key up: Ctrl
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}
