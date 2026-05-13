#include "config_dialog.h"
#include "../resources/resource.h"
#include <commctrl.h>
#include <windowsx.h>
#include <vector>
#include <string>

// We create dialogs programmatically using dialog templates in memory
// since we don't want to rely on .rc dialog resources for flexibility

struct ConfigDlgData {
    Settings* settings;
    bool changed;
};

// ---- General Page ----
static INT_PTR CALLBACK GeneralPageProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ConfigDlgData* data = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        PROPSHEETPAGE* psp = (PROPSHEETPAGE*)lParam;
        data = (ConfigDlgData*)psp->lParam;

        // Create controls manually
        CreateWindowW(L"STATIC", L"Max history size:",
            WS_CHILD | WS_VISIBLE, 20, 20, 130, 20, hDlg, nullptr,
            GetModuleHandle(nullptr), nullptr);

        HWND hEdit = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
            160, 18, 60, 22, hDlg, (HMENU)(INT_PTR)IDC_HISTORY_SIZE,
            GetModuleHandle(nullptr), nullptr);
        SetWindowTextW(hEdit, std::to_wstring(data->settings->maxHistorySize).c_str());

        HWND hSpin = CreateWindowW(UPDOWN_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_AUTOBUDDY | UDS_ARROWKEYS,
            220, 18, 20, 22, hDlg, (HMENU)(INT_PTR)IDC_HISTORY_SIZE_SPIN,
            GetModuleHandle(nullptr), nullptr);
        SendMessage(hSpin, UDM_SETRANGE32, 1, 500);
        SendMessage(hSpin, UDM_SETPOS32, 0, data->settings->maxHistorySize);

        HWND hIgnoreDup = CreateWindowW(L"BUTTON", L"Ignore duplicates",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 55, 200, 20, hDlg, (HMENU)(INT_PTR)IDC_IGNORE_DUPLICATES,
            GetModuleHandle(nullptr), nullptr);
        Button_SetCheck(hIgnoreDup, data->settings->ignoreDuplicates ? BST_CHECKED : BST_UNCHECKED);

        HWND hSaveHist = CreateWindowW(L"BUTTON", L"Save history between sessions",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 80, 250, 20, hDlg, (HMENU)(INT_PTR)IDC_SAVE_HISTORY,
            GetModuleHandle(nullptr), nullptr);
        Button_SetCheck(hSaveHist, data->settings->saveHistory ? BST_CHECKED : BST_UNCHECKED);

        HWND hPurgeBmp = CreateWindowW(L"BUTTON", L"Purge bitmaps on exit",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 105, 250, 20, hDlg, (HMENU)(INT_PTR)IDC_PURGE_BITMAPS,
            GetModuleHandle(nullptr), nullptr);
        Button_SetCheck(hPurgeBmp, data->settings->purgeBitmapsOnExit ? BST_CHECKED : BST_UNCHECKED);

        HWND hAutostart = CreateWindowW(L"BUTTON", L"Run at Windows startup",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 130, 250, 20, hDlg, (HMENU)(INT_PTR)IDC_AUTOSTART,
            GetModuleHandle(nullptr), nullptr);
        Button_SetCheck(hAutostart, data->settings->runAtStartup ? BST_CHECKED : BST_UNCHECKED);

        // Set font for all children
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lp) -> BOOL {
            SendMessage(hwnd, WM_SETFONT, (WPARAM)lp, TRUE);
            return TRUE;
        }, (LPARAM)hFont);

        return TRUE;
    }
    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->code == PSN_APPLY) {
            wchar_t buf[32];
            GetDlgItemTextW(hDlg, IDC_HISTORY_SIZE, buf, 32);
            int size = _wtoi(buf);
            if (size >= 1 && size <= 500) data->settings->maxHistorySize = size;

            data->settings->ignoreDuplicates =
                (Button_GetCheck(GetDlgItem(hDlg, IDC_IGNORE_DUPLICATES)) == BST_CHECKED);
            data->settings->saveHistory =
                (Button_GetCheck(GetDlgItem(hDlg, IDC_SAVE_HISTORY)) == BST_CHECKED);
            data->settings->purgeBitmapsOnExit =
                (Button_GetCheck(GetDlgItem(hDlg, IDC_PURGE_BITMAPS)) == BST_CHECKED);

            bool autostart = (Button_GetCheck(GetDlgItem(hDlg, IDC_AUTOSTART)) == BST_CHECKED);
            if (autostart != data->settings->runAtStartup) {
                data->settings->runAtStartup = autostart;
                data->settings->SetAutostart(autostart);
            }

            data->changed = true;
            SetWindowLongPtr(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// ---- Popup Page ----
static INT_PTR CALLBACK PopupPageProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ConfigDlgData* data = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        PROPSHEETPAGE* psp = (PROPSHEETPAGE*)lParam;
        data = (ConfigDlgData*)psp->lParam;

        CreateWindowW(L"STATIC", L"Popup position:",
            WS_CHILD | WS_VISIBLE, 20, 20, 130, 20, hDlg, nullptr,
            GetModuleHandle(nullptr), nullptr);

        HWND hCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            160, 18, 150, 100, hDlg, (HMENU)(INT_PTR)IDC_POPUP_POSITION,
            GetModuleHandle(nullptr), nullptr);
        ComboBox_AddString(hCombo, L"At caret position");
        ComboBox_AddString(hCombo, L"At mouse position");
        ComboBox_SetCurSel(hCombo, data->settings->useCaretPosition ? 0 : 1);

        HWND hPreview = CreateWindowW(L"BUTTON", L"Show preview tooltips",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 55, 250, 20, hDlg, (HMENU)(INT_PTR)IDC_SHOW_PREVIEW,
            GetModuleHandle(nullptr), nullptr);
        Button_SetCheck(hPreview, data->settings->showPreviewTooltips ? BST_CHECKED : BST_UNCHECKED);

        HWND hAccel = CreateWindowW(L"BUTTON", L"Show numeric accelerators (1-9, 0)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 80, 300, 20, hDlg, (HMENU)(INT_PTR)IDC_SHOW_ACCELERATORS,
            GetModuleHandle(nullptr), nullptr);
        Button_SetCheck(hAccel, data->settings->showAccelerators ? BST_CHECKED : BST_UNCHECKED);

        CreateWindowW(L"STATIC", L"Max display length:",
            WS_CHILD | WS_VISIBLE, 20, 115, 130, 20, hDlg, nullptr,
            GetModuleHandle(nullptr), nullptr);

        HWND hLen = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
            160, 113, 60, 22, hDlg, (HMENU)(INT_PTR)IDC_MAX_DISPLAY_LEN,
            GetModuleHandle(nullptr), nullptr);
        SetWindowTextW(hLen, std::to_wstring(data->settings->maxDisplayLength).c_str());

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lp) -> BOOL {
            SendMessage(hwnd, WM_SETFONT, (WPARAM)lp, TRUE);
            return TRUE;
        }, (LPARAM)hFont);

        return TRUE;
    }
    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->code == PSN_APPLY) {
            data->settings->useCaretPosition =
                (ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_POPUP_POSITION)) == 0);
            data->settings->showPreviewTooltips =
                (Button_GetCheck(GetDlgItem(hDlg, IDC_SHOW_PREVIEW)) == BST_CHECKED);
            data->settings->showAccelerators =
                (Button_GetCheck(GetDlgItem(hDlg, IDC_SHOW_ACCELERATORS)) == BST_CHECKED);

            wchar_t buf[32];
            GetDlgItemTextW(hDlg, IDC_MAX_DISPLAY_LEN, buf, 32);
            int len = _wtoi(buf);
            if (len >= 10 && len <= 200) data->settings->maxDisplayLength = len;

            data->changed = true;
            SetWindowLongPtr(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// ---- Hotkeys Page ----
static INT_PTR CALLBACK HotkeysPageProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ConfigDlgData* data = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        PROPSHEETPAGE* psp = (PROPSHEETPAGE*)lParam;
        data = (ConfigDlgData*)psp->lParam;

        CreateWindowW(L"STATIC", L"Popup hotkey:",
            WS_CHILD | WS_VISIBLE, 20, 20, 130, 20, hDlg, nullptr,
            GetModuleHandle(nullptr), nullptr);

        HWND hHotkey = CreateWindowW(HOTKEY_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            160, 18, 180, 24, hDlg, (HMENU)(INT_PTR)IDC_HOTKEY_POPUP,
            GetModuleHandle(nullptr), nullptr);

        // Convert MOD_ flags to HOTKEYF_ flags for the control
        WORD hkMod = 0;
        if (data->settings->hotkeyModifiers & MOD_SHIFT) hkMod |= HOTKEYF_SHIFT;
        if (data->settings->hotkeyModifiers & MOD_CONTROL) hkMod |= HOTKEYF_CONTROL;
        if (data->settings->hotkeyModifiers & MOD_ALT) hkMod |= HOTKEYF_ALT;
        SendMessage(hHotkey, HKM_SETHOTKEY, MAKEWORD(data->settings->hotkeyVK, hkMod), 0);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lp) -> BOOL {
            SendMessage(hwnd, WM_SETFONT, (WPARAM)lp, TRUE);
            return TRUE;
        }, (LPARAM)hFont);

        return TRUE;
    }
    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->code == PSN_APPLY) {
            HWND hHotkey = GetDlgItem(hDlg, IDC_HOTKEY_POPUP);
            LRESULT hk = SendMessage(hHotkey, HKM_GETHOTKEY, 0, 0);
            BYTE vk = LOBYTE(LOWORD(hk));
            BYTE mod = HIBYTE(LOWORD(hk));

            if (vk != 0) {
                UINT modifiers = 0;
                if (mod & HOTKEYF_SHIFT) modifiers |= MOD_SHIFT;
                if (mod & HOTKEYF_CONTROL) modifiers |= MOD_CONTROL;
                if (mod & HOTKEYF_ALT) modifiers |= MOD_ALT;
                data->settings->hotkeyModifiers = modifiers;
                data->settings->hotkeyVK = vk;
            }

            data->changed = true;
            SetWindowLongPtr(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// Helper: Create an in-memory dialog template (empty dialog)
static std::vector<BYTE> CreateEmptyDialogTemplate(const wchar_t* title, int width, int height) {
    std::vector<BYTE> buf;

    // DLGTEMPLATE
    auto push = [&](const void* data, size_t size) {
        size_t pos = buf.size();
        buf.resize(pos + size);
        memcpy(buf.data() + pos, data, size);
    };
    auto pushWord = [&](WORD w) { push(&w, 2); };
    auto pushDword = [&](DWORD d) { push(&d, 4); };
    auto pushString = [&](const wchar_t* s) {
        size_t len = wcslen(s) + 1;
        push(s, len * 2);
    };

    // Align to DWORD
    auto align4 = [&]() {
        while (buf.size() % 4 != 0) buf.push_back(0);
    };

    DWORD style = WS_CHILD | WS_VISIBLE | DS_CONTROL;
    pushDword(style);          // style
    pushDword(0);              // dwExtendedStyle
    pushWord(0);               // cdit (no controls - we add them in WM_INITDIALOG)
    pushWord(0);               // x
    pushWord(0);               // y
    pushWord((WORD)width);     // cx
    pushWord((WORD)height);    // cy
    pushWord(0);               // menu (none)
    pushWord(0);               // class (default)
    pushString(title);         // title

    return buf;
}

bool ShowConfigDialog(HWND hwndParent, Settings& settings) {
    ConfigDlgData data = { &settings, false };

    auto dlgGeneral = CreateEmptyDialogTemplate(L"General", 350, 200);
    auto dlgPopup = CreateEmptyDialogTemplate(L"Popup", 350, 200);
    auto dlgHotkeys = CreateEmptyDialogTemplate(L"Hotkeys", 350, 200);

    PROPSHEETPAGEW pages[3] = {};

    pages[0].dwSize = sizeof(PROPSHEETPAGEW);
    pages[0].dwFlags = PSP_DLGINDIRECT;
    pages[0].pResource = (DLGTEMPLATE*)dlgGeneral.data();
    pages[0].pfnDlgProc = GeneralPageProc;
    pages[0].pszTitle = L"General";
    pages[0].dwFlags |= PSP_USETITLE;
    pages[0].lParam = (LPARAM)&data;

    pages[1].dwSize = sizeof(PROPSHEETPAGEW);
    pages[1].dwFlags = PSP_DLGINDIRECT | PSP_USETITLE;
    pages[1].pResource = (DLGTEMPLATE*)dlgPopup.data();
    pages[1].pfnDlgProc = PopupPageProc;
    pages[1].pszTitle = L"Popup";
    pages[1].lParam = (LPARAM)&data;

    pages[2].dwSize = sizeof(PROPSHEETPAGEW);
    pages[2].dwFlags = PSP_DLGINDIRECT | PSP_USETITLE;
    pages[2].pResource = (DLGTEMPLATE*)dlgHotkeys.data();
    pages[2].pfnDlgProc = HotkeysPageProc;
    pages[2].pszTitle = L"Hotkeys";
    pages[2].lParam = (LPARAM)&data;

    PROPSHEETHEADERW psh = {};
    psh.dwSize = sizeof(psh);
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
    psh.hwndParent = hwndParent;
    psh.hInstance = GetModuleHandle(nullptr);
    psh.pszCaption = L"XClip Settings";
    psh.nPages = 3;
    psh.ppsp = pages;

    INT_PTR result = PropertySheetW(&psh);

    if (data.changed) {
        settings.Save();
    }

    return data.changed;
}
