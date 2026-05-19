#include "notes.h"
#include "utils.h"
#include "../resources/resource.h"
#include <windowsx.h>
#include <algorithm>
#include <vector>

// ---- NotesManager implementation ----

void NotesManager::AddNote(const std::wstring& title, const std::wstring& content) {
    Note n;
    n.title = title;
    n.content = content;
    notes_.push_back(std::move(n));
}

void NotesManager::UpdateNote(int index, const std::wstring& title, const std::wstring& content) {
    if (index < 0 || index >= (int)notes_.size()) return;
    notes_[index].title = title;
    notes_[index].content = content;
    GetSystemTimeAsFileTime(&notes_[index].modified);
}

void NotesManager::RemoveNote(int index) {
    if (index < 0 || index >= (int)notes_.size()) return;
    notes_.erase(notes_.begin() + index);
}

// ---- Persistence ----
// Format: magic(4) version(4) count(4) [ titleLen(4) title(bytes) contentLen(4) content(bytes) created(8) modified(8) ]

static const DWORD kNotesMagic = 0x544F4E58; // "XNOT"
static const DWORD kNotesVersion = 1;

bool NotesManager::SaveToFile(const std::wstring& path) const {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written;
    auto writeU32 = [&](DWORD v) { WriteFile(hFile, &v, 4, &written, nullptr); };
    auto writeBytes = [&](const void* data, DWORD size) { WriteFile(hFile, data, size, &written, nullptr); };

    writeU32(kNotesMagic);
    writeU32(kNotesVersion);
    writeU32((DWORD)notes_.size());

    for (auto& note : notes_) {
        DWORD titleLen = (DWORD)note.title.size();
        writeU32(titleLen);
        if (titleLen > 0) writeBytes(note.title.c_str(), titleLen * sizeof(wchar_t));

        DWORD contentLen = (DWORD)note.content.size();
        writeU32(contentLen);
        if (contentLen > 0) writeBytes(note.content.c_str(), contentLen * sizeof(wchar_t));

        writeBytes(&note.created, sizeof(FILETIME));
        writeBytes(&note.modified, sizeof(FILETIME));
    }

    CloseHandle(hFile);
    return true;
}

bool NotesManager::LoadFromFile(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD bytesRead;
    auto readU32 = [&]() -> DWORD {
        DWORD v = 0;
        ReadFile(hFile, &v, 4, &bytesRead, nullptr);
        return v;
    };
    auto readBytes = [&](void* buf, DWORD size) -> bool {
        return ReadFile(hFile, buf, size, &bytesRead, nullptr) && bytesRead == size;
    };

    DWORD magic = readU32();
    DWORD version = readU32();
    if (magic != kNotesMagic || version != kNotesVersion) {
        CloseHandle(hFile);
        return false;
    }

    DWORD count = readU32();
    if (count > 100000) { CloseHandle(hFile); return false; }

    notes_.clear();
    for (DWORD i = 0; i < count; i++) {
        Note note;
        DWORD titleLen = readU32();
        if (titleLen > 0 && titleLen < 10000000) {
            note.title.resize(titleLen);
            if (!readBytes(note.title.data(), titleLen * sizeof(wchar_t))) break;
        }
        DWORD contentLen = readU32();
        if (contentLen > 0 && contentLen < 10000000) {
            note.content.resize(contentLen);
            if (!readBytes(note.content.data(), contentLen * sizeof(wchar_t))) break;
        }
        readBytes(&note.created, sizeof(FILETIME));
        readBytes(&note.modified, sizeof(FILETIME));
        notes_.push_back(std::move(note));
    }

    CloseHandle(hFile);
    return true;
}

// ---- Note Editor Dialog ----

struct NoteEditorData {
    std::wstring* title;
    std::wstring* content;
    bool isNew;
    bool saved;
};

static INT_PTR CALLBACK NoteEditorProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    NoteEditorData* data = (NoteEditorData*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        data = (NoteEditorData*)lParam;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        HINSTANCE hInst = GetModuleHandle(nullptr);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // Title label + edit
        HWND hLbl = CreateWindowW(L"STATIC", L"Title:",
            WS_CHILD | WS_VISIBLE, 10, 12, 40, 20, hDlg, nullptr, hInst, nullptr);
        SendMessage(hLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hTitle = CreateWindowW(L"EDIT", data->title->c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            55, 10, 420, 22, hDlg, (HMENU)(INT_PTR)IDC_NOTES_TITLE, hInst, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Content edit (multiline)
        HFONT hMonoFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        HWND hContent = CreateWindowW(L"EDIT", data->content->c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            10, 42, 465, 280, hDlg, (HMENU)(INT_PTR)IDC_NOTES_CONTENT, hInst, nullptr);
        SendMessage(hContent, WM_SETFONT, (WPARAM)(hMonoFont ? hMonoFont : hFont), TRUE);

        // Buttons
        HWND hSave = CreateWindowW(L"BUTTON", L"Save",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            310, 332, 75, 28, hDlg, (HMENU)IDOK, hInst, nullptr);
        SendMessage(hSave, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hCancel = CreateWindowW(L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE,
            395, 332, 75, 28, hDlg, (HMENU)IDCANCEL, hInst, nullptr);
        SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

        SetWindowTextW(hDlg, data->isNew ? L"New Note" : L"Edit Note");

        Utils::RestoreWindowPos(hDlg, L"WndPos_NoteEditor");

        // Trigger initial layout
        SendMessage(hDlg, WM_SIZE, SIZE_RESTORED, 0);

        SetFocus(data->isNew ? hTitle : hContent);
        return FALSE;
    }

    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) break;
        RECT rc;
        GetClientRect(hDlg, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // Title edit: stretch width
        MoveWindow(GetDlgItem(hDlg, IDC_NOTES_TITLE), 55, 10, w - 65, 22, TRUE);
        // Content: fill remaining space, leave room for buttons
        MoveWindow(GetDlgItem(hDlg, IDC_NOTES_CONTENT), 10, 42, w - 20, h - 82, TRUE);
        // Buttons: anchored to bottom-right
        MoveWindow(GetDlgItem(hDlg, IDOK), w - 170, h - 34, 75, 28, TRUE);
        MoveWindow(GetDlgItem(hDlg, IDCANCEL), w - 85, h - 34, 75, 28, TRUE);
        InvalidateRect(hDlg, nullptr, TRUE);
        return TRUE;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 350;
        mmi->ptMinTrackSize.y = 200;
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            wchar_t buf[256] = {};
            GetDlgItemTextW(hDlg, IDC_NOTES_TITLE, buf, 256);
            *data->title = buf;

            // Get content length, then allocate
            int len = GetWindowTextLengthW(GetDlgItem(hDlg, IDC_NOTES_CONTENT));
            if (len > 0) {
                std::wstring content(len + 1, L'\0');
                GetDlgItemTextW(hDlg, IDC_NOTES_CONTENT, content.data(), len + 1);
                content.resize(len);
                *data->content = content;
            } else {
                data->content->clear();
            }

            if (data->title->empty()) {
                // Auto-generate title from first line
                if (!data->content->empty()) {
                    auto pos = data->content->find(L'\n');
                    *data->title = Utils::Truncate(data->content->substr(0,
                        pos != std::wstring::npos ? pos : data->content->size()), 50);
                } else {
                    *data->title = L"Untitled";
                }
            }

            data->saved = true;
            Utils::SaveWindowPos(hDlg, L"WndPos_NoteEditor");
            EndDialog(hDlg, IDOK);
            break;
        }
        case IDCANCEL:
            Utils::SaveWindowPos(hDlg, L"WndPos_NoteEditor");
            EndDialog(hDlg, IDCANCEL);
            break;
        }
        return TRUE;

    case WM_CLOSE:
        Utils::SaveWindowPos(hDlg, L"WndPos_NoteEditor");
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

static std::vector<BYTE> CreateEditorDialogTemplate() {
    std::vector<BYTE> buf;
    auto pushWord = [&](WORD w) { buf.insert(buf.end(), (BYTE*)&w, (BYTE*)&w + 2); };
    auto pushDword = [&](DWORD d) { buf.insert(buf.end(), (BYTE*)&d, (BYTE*)&d + 4); };
    auto pushString = [&](const wchar_t* s) {
        size_t len = wcslen(s) + 1;
        buf.insert(buf.end(), (BYTE*)s, (BYTE*)s + len * 2);
    };

    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | DS_MODALFRAME | DS_CENTER;
    pushDword(style);
    pushDword(0);
    pushWord(0);
    pushWord(0);
    pushWord(0);
    pushWord(330);
    pushWord(250);
    pushWord(0);
    pushWord(0);
    pushString(L"Note Editor");
    return buf;
}

bool ShowNoteEditor(HWND hwndParent, std::wstring& title, std::wstring& content, bool isNew) {
    NoteEditorData data = { &title, &content, isNew, false };
    auto dlgTemplate = CreateEditorDialogTemplate();
    DialogBoxIndirectParamW(GetModuleHandle(nullptr),
        (DLGTEMPLATE*)dlgTemplate.data(), hwndParent, NoteEditorProc, (LPARAM)&data);
    return data.saved;
}

// ---- Notes Manager Dialog ----

struct NotesManagerData {
    NotesManager* notes;
};

static void RefreshNotesList(HWND hList, NotesManager& notes) {
    ListBox_ResetContent(hList);
    for (int i = 0; i < notes.GetCount(); i++) {
        ListBox_AddString(hList, notes.GetNote(i).title.c_str());
    }
}

static INT_PTR CALLBACK NotesManagerProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    NotesManagerData* data = (NotesManagerData*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        data = (NotesManagerData*)lParam;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        HINSTANCE hInst = GetModuleHandle(nullptr);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        HWND hList = CreateWindowW(L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            10, 10, 300, 280, hDlg, (HMENU)(INT_PTR)IDC_NOTES_LIST, hInst, nullptr);
        SendMessage(hList, WM_SETFONT, (WPARAM)hFont, TRUE);

        auto makeBtn = [&](const wchar_t* text, int id, int y) {
            HWND h = CreateWindowW(L"BUTTON", text,
                WS_CHILD | WS_VISIBLE, 320, y, 85, 28, hDlg, (HMENU)(INT_PTR)id, hInst, nullptr);
            SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        };

        makeBtn(L"&New", IDC_NOTES_NEW, 10);
        makeBtn(L"&Edit", IDC_NOTES_EDIT, 45);
        makeBtn(L"&Delete", IDC_NOTES_DELETE, 80);
        makeBtn(L"&Paste", IDC_NOTES_PASTE, 130);

        HWND hClose = CreateWindowW(L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE, 320, 260, 85, 28, hDlg, (HMENU)IDCANCEL, hInst, nullptr);
        SendMessage(hClose, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Initial layout
        SendMessage(hDlg, WM_SIZE, SIZE_RESTORED, 0);

        Utils::RestoreWindowPos(hDlg, L"WndPos_NotesManager");
        // Re-trigger layout after position restore
        SendMessage(hDlg, WM_SIZE, SIZE_RESTORED, 0);

        RefreshNotesList(hList, *data->notes);
        return TRUE;
    }

    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) break;
        RECT rc;
        GetClientRect(hDlg, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // List: fill left area, leave 105px for buttons on the right
        HWND hList = GetDlgItem(hDlg, IDC_NOTES_LIST);
        MoveWindow(hList, 10, 10, w - 115, h - 20, TRUE);

        // Buttons: anchored to right edge
        int btnX = w - 95;
        MoveWindow(GetDlgItem(hDlg, IDC_NOTES_NEW), btnX, 10, 85, 28, TRUE);
        MoveWindow(GetDlgItem(hDlg, IDC_NOTES_EDIT), btnX, 45, 85, 28, TRUE);
        MoveWindow(GetDlgItem(hDlg, IDC_NOTES_DELETE), btnX, 80, 85, 28, TRUE);
        MoveWindow(GetDlgItem(hDlg, IDC_NOTES_PASTE), btnX, 130, 85, 28, TRUE);
        MoveWindow(GetDlgItem(hDlg, IDCANCEL), btnX, h - 38, 85, 28, TRUE);
        InvalidateRect(hDlg, nullptr, TRUE);
        return TRUE;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 300;
        mmi->ptMinTrackSize.y = 200;
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_NOTES_NEW: {
            std::wstring title, content;
            if (ShowNoteEditor(hDlg, title, content, true)) {
                data->notes->AddNote(title, content);
                RefreshNotesList(GetDlgItem(hDlg, IDC_NOTES_LIST), *data->notes);
            }
            break;
        }
        case IDC_NOTES_EDIT: {
            int sel = ListBox_GetCurSel(GetDlgItem(hDlg, IDC_NOTES_LIST));
            if (sel >= 0 && sel < data->notes->GetCount()) {
                std::wstring title = data->notes->GetNote(sel).title;
                std::wstring content = data->notes->GetNote(sel).content;
                if (ShowNoteEditor(hDlg, title, content, false)) {
                    data->notes->UpdateNote(sel, title, content);
                    RefreshNotesList(GetDlgItem(hDlg, IDC_NOTES_LIST), *data->notes);
                }
            }
            break;
        }
        case IDC_NOTES_DELETE: {
            int sel = ListBox_GetCurSel(GetDlgItem(hDlg, IDC_NOTES_LIST));
            if (sel >= 0 && sel < data->notes->GetCount()) {
                int result = MessageBoxW(hDlg,
                    (L"Delete note \"" + data->notes->GetNote(sel).title + L"\"?").c_str(),
                    L"Delete Note", MB_YESNO | MB_ICONQUESTION);
                if (result == IDYES) {
                    data->notes->RemoveNote(sel);
                    RefreshNotesList(GetDlgItem(hDlg, IDC_NOTES_LIST), *data->notes);
                }
            }
            break;
        }
        case IDC_NOTES_PASTE: {
            // Paste note content to clipboard
            int sel = ListBox_GetCurSel(GetDlgItem(hDlg, IDC_NOTES_LIST));
            if (sel >= 0 && sel < data->notes->GetCount()) {
                const std::wstring& content = data->notes->GetNote(sel).content;
                if (OpenClipboard(hDlg)) {
                    EmptyClipboard();
                    size_t bytes = (content.size() + 1) * sizeof(wchar_t);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
                    if (hMem) {
                        void* p = GlobalLock(hMem);
                        if (p) {
                            memcpy(p, content.c_str(), bytes);
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_UNICODETEXT, hMem);
                        }
                    }
                    CloseClipboard();
                }
            }
            break;
        }
        case IDC_NOTES_LIST:
            if (HIWORD(wParam) == LBN_DBLCLK) {
                // Double-click = edit
                SendMessage(hDlg, WM_COMMAND, IDC_NOTES_EDIT, 0);
            }
            break;
        case IDCANCEL:
            Utils::SaveWindowPos(hDlg, L"WndPos_NotesManager");
            EndDialog(hDlg, IDCANCEL);
            break;
        }
        return TRUE;

    case WM_CLOSE:
        Utils::SaveWindowPos(hDlg, L"WndPos_NotesManager");
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

static std::vector<BYTE> CreateManagerDialogTemplate() {
    std::vector<BYTE> buf;
    auto pushWord = [&](WORD w) { buf.insert(buf.end(), (BYTE*)&w, (BYTE*)&w + 2); };
    auto pushDword = [&](DWORD d) { buf.insert(buf.end(), (BYTE*)&d, (BYTE*)&d + 4); };
    auto pushString = [&](const wchar_t* s) {
        size_t len = wcslen(s) + 1;
        buf.insert(buf.end(), (BYTE*)s, (BYTE*)s + len * 2);
    };

    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | DS_CENTER;
    pushDword(style);
    pushDword(0);
    pushWord(0);
    pushWord(0);
    pushWord(0);
    pushWord(280);
    pushWord(200);
    pushWord(0);
    pushWord(0);
    pushString(L"XClip - Notes");
    return buf;
}

void ShowNotesManager(HWND hwndParent, NotesManager& notes) {
    NotesManagerData data = { &notes };
    auto dlgTemplate = CreateManagerDialogTemplate();
    DialogBoxIndirectParamW(GetModuleHandle(nullptr),
        (DLGTEMPLATE*)dlgTemplate.data(), hwndParent, NotesManagerProc, (LPARAM)&data);
}
