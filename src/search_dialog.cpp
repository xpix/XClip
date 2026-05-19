#include "search_dialog.h"
#include "utils.h"
#include "../resources/resource.h"
#include <windowsx.h>
#include <vector>

struct SearchDlgData {
    const History* history;
    int selectedIndex;
    std::vector<int> filteredIndices;
};

static void UpdateSearchResults(HWND hDlg, SearchDlgData* data) {
    HWND hList = GetDlgItem(hDlg, IDC_SEARCH_LIST);
    HWND hEdit = GetDlgItem(hDlg, IDC_SEARCH_EDIT);

    wchar_t query[256] = {};
    GetWindowTextW(hEdit, query, 256);

    ListBox_ResetContent(hList);
    data->filteredIndices.clear();

    if (wcslen(query) == 0) {
        // Show all entries
        for (int i = 0; i < data->history->GetCount(); i++) {
            data->filteredIndices.push_back(i);
            std::wstring display = data->history->GetEntry(i).GetDisplayText(80);
            ListBox_AddString(hList, display.c_str());
        }
    } else {
        data->filteredIndices = data->history->Search(query);
        for (int idx : data->filteredIndices) {
            std::wstring display = data->history->GetEntry(idx).GetDisplayText(80);
            ListBox_AddString(hList, display.c_str());
        }
    }
}

static INT_PTR CALLBACK SearchDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    SearchDlgData* data = (SearchDlgData*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        data = (SearchDlgData*)lParam;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        // Create controls
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HINSTANCE hInst = GetModuleHandle(nullptr);

        CreateWindowW(L"STATIC", L"Search:",
            WS_CHILD | WS_VISIBLE, 10, 12, 50, 20, hDlg, nullptr, hInst, nullptr);

        HWND hEdit = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            65, 10, 310, 22, hDlg, (HMENU)(INT_PTR)IDC_SEARCH_EDIT, hInst, nullptr);

        HWND hList = CreateWindowW(L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            10, 42, 365, 240, hDlg, (HMENU)(INT_PTR)IDC_SEARCH_LIST, hInst, nullptr);

        HWND hOk = CreateWindowW(L"BUTTON", L"Paste",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            210, 292, 75, 28, hDlg, (HMENU)IDOK, hInst, nullptr);

        HWND hCancel = CreateWindowW(L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE,
            295, 292, 75, 28, hDlg, (HMENU)IDCANCEL, hInst, nullptr);

        // Set font
        HWND children[] = { hEdit, hList, hOk, hCancel };
        for (HWND h : children) SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Trigger initial layout to match actual window size
        SendMessage(hDlg, WM_SIZE, SIZE_RESTORED, 0);

        Utils::RestoreWindowPos(hDlg, L"WndPos_Search");
        // Re-trigger layout after position restore
        SendMessage(hDlg, WM_SIZE, SIZE_RESTORED, 0);

        // Populate list
        UpdateSearchResults(hDlg, data);

        SetFocus(hEdit);
        return FALSE; // We set focus manually
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SEARCH_EDIT:
            if (HIWORD(wParam) == EN_CHANGE) {
                UpdateSearchResults(hDlg, data);
            }
            break;

        case IDC_SEARCH_LIST:
            if (HIWORD(wParam) == LBN_DBLCLK) {
                // Double-click = select and paste
                int sel = ListBox_GetCurSel(GetDlgItem(hDlg, IDC_SEARCH_LIST));
                if (sel >= 0 && sel < (int)data->filteredIndices.size()) {
                    data->selectedIndex = data->filteredIndices[sel];
                    Utils::SaveWindowPos(hDlg, L"WndPos_Search");
                    EndDialog(hDlg, IDOK);
                }
            }
            break;

        case IDOK: {
            int sel = ListBox_GetCurSel(GetDlgItem(hDlg, IDC_SEARCH_LIST));
            if (sel >= 0 && sel < (int)data->filteredIndices.size()) {
                data->selectedIndex = data->filteredIndices[sel];
                Utils::SaveWindowPos(hDlg, L"WndPos_Search");
                EndDialog(hDlg, IDOK);
            }
            break;
        }

        case IDCANCEL:
            Utils::SaveWindowPos(hDlg, L"WndPos_Search");
            EndDialog(hDlg, IDCANCEL);
            break;
        }
        return TRUE;

    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) break;
        RECT rc;
        GetClientRect(hDlg, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HWND hEdit = GetDlgItem(hDlg, IDC_SEARCH_EDIT);
        HWND hList = GetDlgItem(hDlg, IDC_SEARCH_LIST);

        // Edit field: stretch width, keep at top
        MoveWindow(hEdit, 65, 10, w - 85, 22, TRUE);
        // List: fill remaining space, leave room for buttons at bottom
        MoveWindow(hList, 10, 42, w - 20, h - 82, TRUE);
        // Buttons: anchored to bottom-right
        HWND hOk = GetDlgItem(hDlg, IDOK);
        HWND hCancel = GetDlgItem(hDlg, IDCANCEL);
        MoveWindow(hOk, w - 170, h - 34, 75, 28, TRUE);
        MoveWindow(hCancel, w - 85, h - 34, 75, 28, TRUE);
        InvalidateRect(hDlg, nullptr, TRUE);
        return TRUE;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 300;
        mmi->ptMinTrackSize.y = 200;
        return TRUE;
    }

    case WM_CLOSE:
        Utils::SaveWindowPos(hDlg, L"WndPos_Search");
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// Helper to create an in-memory dialog template for the search dialog
static std::vector<BYTE> CreateSearchDialogTemplate() {
    std::vector<BYTE> buf;

    auto pushWord = [&](WORD w) {
        size_t pos = buf.size();
        buf.resize(pos + 2);
        memcpy(buf.data() + pos, &w, 2);
    };
    auto pushDword = [&](DWORD d) {
        size_t pos = buf.size();
        buf.resize(pos + 4);
        memcpy(buf.data() + pos, &d, 4);
    };
    auto pushString = [&](const wchar_t* s) {
        size_t len = wcslen(s) + 1;
        size_t pos = buf.size();
        buf.resize(pos + len * 2);
        memcpy(buf.data() + pos, s, len * 2);
    };

    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | DS_CENTER;
    pushDword(style);
    pushDword(0);           // extended style
    pushWord(0);            // no controls (added in WM_INITDIALOG)
    pushWord(0);            // x
    pushWord(0);            // y
    pushWord(260);          // width (dialog units)
    pushWord(220);          // height
    pushWord(0);            // menu
    pushWord(0);            // class
    pushString(L"XClip - Search History");

    return buf;
}

int ShowSearchDialog(HWND hwndParent, const History& history) {
    SearchDlgData data = {};
    data.history = &history;
    data.selectedIndex = -1;

    auto dlgTemplate = CreateSearchDialogTemplate();

    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandle(nullptr),
        (DLGTEMPLATE*)dlgTemplate.data(),
        hwndParent,
        SearchDlgProc,
        (LPARAM)&data);

    if (result == IDOK) {
        return data.selectedIndex;
    }
    return -1;
}
