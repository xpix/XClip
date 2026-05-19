#include "popup.h"
#include "utils.h"
#include "../resources/resource.h"
#include <commctrl.h>
#include <algorithm>

static const wchar_t* kPreviewClass = L"XCLIP_PREVIEW";
static bool sPreviewClassRegistered = false;

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBITMAP hbm = (HBITMAP)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (hbm) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);
            BITMAP bm;
            GetObject(hbm, sizeof(bm), &bm);
            SetStretchBltMode(hdc, HALFTONE);
            StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
            SelectObject(hdcMem, hOld);
            DeleteDC(hdcMem);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

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
        wsprintfW(buf, L"(%d more items... Search)", count - maxItems);
        AppendMenuW(hMenu, MF_STRING, IDM_CLIP_SEARCH_MORE, buf);
    }
}

int PopupMenu::Show(HWND hwnd, const History& history, const Settings& settings) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return -1;

    currentHistory_ = &history;
    showTooltips_ = settings.showPreviewTooltips;

    if (showTooltips_) {
        CreateTooltipWindow(hwnd);
    }

    BuildMenu(hMenu, history, settings);

    POINT pt = GetPopupPosition(settings.useCaretPosition);

    // Ensure our window is foreground so the menu dismisses properly
    SetForegroundWindow(hwnd);

    int cmd = TrackPopupMenu(hMenu,
        TPM_RETURNCMD | TPM_LEFTBUTTON,
        pt.x, pt.y, 0, hwnd, nullptr);

    OnMenuClose();
    DestroyMenu(hMenu);
    currentHistory_ = nullptr;

    // Required after TrackPopupMenu to dismiss properly
    PostMessage(hwnd, WM_NULL, 0, 0);

    if (cmd >= IDM_CLIP_BASE && cmd <= IDM_CLIP_MAX) {
        return cmd - IDM_CLIP_BASE;
    }

    if (cmd == IDM_CLIP_SEARCH_MORE) {
        return -2; // Signal to open search dialog
    }

    return -1;
}

void PopupMenu::CreateTooltipWindow(HWND hwnd) {
    if (hwndTooltip_) {
        DestroyWindow(hwndTooltip_);
        hwndTooltip_ = nullptr;
    }

    hwndTooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

    if (hwndTooltip_) {
        SendMessage(hwndTooltip_, TTM_SETMAXTIPWIDTH, 0, 500);
        // Add a dummy tool for manual control
        TOOLINFOW ti = {};
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_TRACK | TTF_ABSOLUTE;
        ti.hwnd = hwnd;
        ti.uId = 1;
        ti.lpszText = (LPWSTR)L"";
        SendMessage(hwndTooltip_, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    }
}

void PopupMenu::ShowTooltipForItem(int index) {
    if (!hwndTooltip_ || !currentHistory_) return;
    if (index < 0 || index >= currentHistory_->GetCount()) {
        // Hide tooltip
        TOOLINFOW ti = {};
        ti.cbSize = sizeof(ti);
        ti.hwnd = GetParent(hwndTooltip_);
        ti.uId = 1;
        SendMessage(hwndTooltip_, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti);
        HidePreview();
        return;
    }

    const ClipEntry& entry = currentHistory_->GetEntry(index);

    if (entry.type == ClipType::Bitmap) {
        // Hide text tooltip, show bitmap preview
        TOOLINFOW ti = {};
        ti.cbSize = sizeof(ti);
        ti.hwnd = GetParent(hwndTooltip_);
        ti.uId = 1;
        SendMessage(hwndTooltip_, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti);
        ShowPreviewForBitmap(index);
        return;
    }

    // Hide bitmap preview for non-bitmap items
    HidePreview();

    // Build preview text (show more than the menu item)
    std::wstring preview;
    if (entry.type == ClipType::Text) {
        preview = Utils::Truncate(entry.text, 500);
    } else if (entry.type == ClipType::Files) {
        for (auto& f : entry.files) {
            if (!preview.empty()) preview += L"\r\n";
            preview += f;
        }
        preview = Utils::Truncate(preview, 500);
    }

    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = GetParent(hwndTooltip_);
    ti.uId = 1;
    ti.lpszText = (LPWSTR)preview.c_str();
    SendMessage(hwndTooltip_, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);

    // Position tooltip near the mouse
    POINT pt;
    GetCursorPos(&pt);
    SendMessage(hwndTooltip_, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x + 20, pt.y));
    SendMessage(hwndTooltip_, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);
}

void PopupMenu::CreatePreviewWindow(HWND hwnd) {
    if (!sPreviewClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PreviewWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = kPreviewClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
        sPreviewClassRegistered = true;
    }

    hwndPreview_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kPreviewClass, nullptr,
        WS_POPUP | WS_BORDER,
        0, 0, 200, 150,
        hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
}

void PopupMenu::ShowPreviewForBitmap(int index) {
    if (!currentHistory_) return;
    const ClipEntry& entry = currentHistory_->GetEntry(index);
    if (entry.type != ClipType::Bitmap || !entry.hBitmap) return;

    if (!hwndPreview_) {
        CreatePreviewWindow(GetParent(hwndTooltip_));
    }
    if (!hwndPreview_) return;

    // Calculate thumbnail size (max 200x200, preserve aspect ratio)
    BITMAP bm;
    GetObject(entry.hBitmap, sizeof(bm), &bm);
    int maxDim = 200;
    int thumbW = bm.bmWidth;
    int thumbH = bm.bmHeight;
    if (thumbW > maxDim || thumbH > maxDim) {
        float scale = (std::min)((float)maxDim / thumbW, (float)maxDim / thumbH);
        thumbW = (int)(thumbW * scale);
        thumbH = (int)(thumbH * scale);
    }

    // Store bitmap reference for painting
    SetWindowLongPtr(hwndPreview_, GWLP_USERDATA, (LONG_PTR)entry.hBitmap);

    // Position near mouse
    POINT pt;
    GetCursorPos(&pt);

    // Adjust for window borders
    RECT adjustRect = { 0, 0, thumbW, thumbH };
    AdjustWindowRectEx(&adjustRect, WS_POPUP | WS_BORDER, FALSE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
    int winW = adjustRect.right - adjustRect.left;
    int winH = adjustRect.bottom - adjustRect.top;

    SetWindowPos(hwndPreview_, HWND_TOPMOST,
        pt.x + 20, pt.y - winH / 2,
        winW, winH,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwndPreview_, nullptr, TRUE);
}

void PopupMenu::HidePreview() {
    if (hwndPreview_) {
        ShowWindow(hwndPreview_, SW_HIDE);
    }
}

void PopupMenu::OnMenuSelect(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    if (!showTooltips_ || !hwndTooltip_) return;

    UINT flags = HIWORD(wParam);
    UINT itemId = LOWORD(wParam);

    if (flags == 0xFFFF && lParam == 0) {
        // Menu closed
        OnMenuClose();
        return;
    }

    if ((flags & MF_HILITE) && !(flags & MF_SEPARATOR) && !(flags & MF_POPUP)) {
        if (itemId >= IDM_CLIP_BASE && itemId <= IDM_CLIP_MAX) {
            ShowTooltipForItem(itemId - IDM_CLIP_BASE);
            return;
        }
    }

    // Hide tooltip for non-clip items
    ShowTooltipForItem(-1);
}

void PopupMenu::OnMenuClose() {
    if (hwndTooltip_) {
        DestroyWindow(hwndTooltip_);
        hwndTooltip_ = nullptr;
    }
    if (hwndPreview_) {
        DestroyWindow(hwndPreview_);
        hwndPreview_ = nullptr;
    }
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
