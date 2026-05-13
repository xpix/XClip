#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <deque>
#include <memory>

enum class ClipType {
    Text,
    Bitmap,
    Files
};

struct ClipEntry {
    ClipType type = ClipType::Text;
    std::wstring text;              // For Text and Files types
    HBITMAP hBitmap = nullptr;      // For Bitmap type (owned)
    std::vector<std::wstring> files; // For Files type
    FILETIME timestamp = {};

    ClipEntry() { GetSystemTimeAsFileTime(&timestamp); }
    ~ClipEntry();

    // Move only
    ClipEntry(ClipEntry&& other) noexcept;
    ClipEntry& operator=(ClipEntry&& other) noexcept;
    ClipEntry(const ClipEntry&) = delete;
    ClipEntry& operator=(const ClipEntry&) = delete;

    // Get display text for menu
    std::wstring GetDisplayText(int maxLen) const;

    // Check if content matches another entry
    bool Matches(const ClipEntry& other) const;

    // Restore this entry to the clipboard
    bool RestoreToClipboard(HWND owner) const;
};

class History {
public:
    History();
    ~History();

    void SetMaxSize(int maxSize);
    int GetMaxSize() const { return maxSize_; }

    // Add a new entry (from current clipboard)
    // Returns true if a new entry was added
    bool CaptureFromClipboard(HWND hwnd, bool ignoreDuplicates);

    // Access entries (0 = most recent)
    int GetCount() const { return (int)entries_.size(); }
    const ClipEntry& GetEntry(int index) const { return *entries_[index]; }

    // Modify
    void RemoveEntry(int index);
    void Clear();
    void ClearBitmaps();
    void UpdateEntryText(int index, const std::wstring& newText);

    // Persistence
    bool SaveToFile(const std::wstring& path, bool purgeBitmaps) const;
    bool LoadFromFile(const std::wstring& path);

    // Search
    std::vector<int> Search(const std::wstring& query) const;

private:
    std::deque<std::unique_ptr<ClipEntry>> entries_;
    int maxSize_ = 25;

    void TrimToSize();
};
