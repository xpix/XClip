#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct Note {
    std::wstring title;
    std::wstring content;
    FILETIME created = {};
    FILETIME modified = {};

    Note() {
        GetSystemTimeAsFileTime(&created);
        modified = created;
    }
};

class NotesManager {
public:
    int GetCount() const { return (int)notes_.size(); }
    const Note& GetNote(int index) const { return notes_[index]; }
    Note& GetNote(int index) { return notes_[index]; }

    void AddNote(const std::wstring& title, const std::wstring& content);
    void UpdateNote(int index, const std::wstring& title, const std::wstring& content);
    void RemoveNote(int index);

    bool SaveToFile(const std::wstring& path) const;
    bool LoadFromFile(const std::wstring& path);

private:
    std::vector<Note> notes_;
};

// Shows the Notes manager dialog (modal)
void ShowNotesManager(HWND hwndParent, NotesManager& notes);

// Shows a text editor for a single note. Returns true if saved.
bool ShowNoteEditor(HWND hwndParent, std::wstring& title, std::wstring& content, bool isNew);
