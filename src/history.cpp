#include "history.h"
#include "utils.h"
#include <algorithm>
#include <fstream>
#include <shlobj.h>
#include <shellapi.h>

// ---- ClipEntry implementation ----

ClipEntry::~ClipEntry() {
    if (hBitmap) {
        DeleteObject(hBitmap);
        hBitmap = nullptr;
    }
}

ClipEntry::ClipEntry(ClipEntry&& other) noexcept
    : type(other.type), text(std::move(other.text)),
      hBitmap(other.hBitmap), files(std::move(other.files)),
      timestamp(other.timestamp) {
    other.hBitmap = nullptr;
}

ClipEntry& ClipEntry::operator=(ClipEntry&& other) noexcept {
    if (this != &other) {
        if (hBitmap) DeleteObject(hBitmap);
        type = other.type;
        text = std::move(other.text);
        hBitmap = other.hBitmap;
        files = std::move(other.files);
        timestamp = other.timestamp;
        other.hBitmap = nullptr;
    }
    return *this;
}

std::wstring ClipEntry::GetDisplayText(int maxLen) const {
    switch (type) {
    case ClipType::Text:
        return Utils::EscapeAmpersand(
            Utils::Truncate(Utils::SingleLine(text), maxLen));
    case ClipType::Bitmap: {
        SIZE sz = Utils::GetBitmapSize(hBitmap);
        wchar_t buf[64];
        wsprintfW(buf, L"[Bitmap %ldx%ld]", sz.cx, sz.cy);
        return buf;
    }
    case ClipType::Files: {
        if (files.empty()) return L"[Files]";
        std::wstring display = files[0];
        // Show just filename
        auto pos = display.find_last_of(L'\\');
        if (pos != std::wstring::npos) display = display.substr(pos + 1);
        if (files.size() > 1) {
            wchar_t buf[32];
            wsprintfW(buf, L" (+%d)", (int)files.size() - 1);
            display += buf;
        }
        return Utils::EscapeAmpersand(Utils::Truncate(display, maxLen));
    }
    }
    return L"<unknown>";
}

bool ClipEntry::Matches(const ClipEntry& other) const {
    if (type != other.type) return false;
    switch (type) {
    case ClipType::Text:
        return text == other.text;
    case ClipType::Files:
        return files == other.files;
    case ClipType::Bitmap: {
        if (!hBitmap || !other.hBitmap) return false;
        BITMAP bm1, bm2;
        GetObject(hBitmap, sizeof(bm1), &bm1);
        GetObject(other.hBitmap, sizeof(bm2), &bm2);
        // Different dimensions → definitely different
        if (bm1.bmWidth != bm2.bmWidth || bm1.bmHeight != bm2.bmHeight) return false;
        // Compare first 4096 bytes of pixel data (fast heuristic, sufficient for screenshots)
        int stride1 = ((bm1.bmWidth * 32 + 31) / 32) * 4;
        size_t dataSize = (size_t)stride1 * bm1.bmHeight;
        size_t cmpSize = (std::min)(dataSize, (size_t)4096);
        std::vector<BYTE> buf1(cmpSize), buf2(cmpSize);
        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(bi);
        bi.biWidth = bm1.bmWidth;
        bi.biHeight = bm1.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        HDC hdc = GetDC(nullptr);
        GetDIBits(hdc, hBitmap, 0, bm1.bmHeight, buf1.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        GetDIBits(hdc, other.hBitmap, 0, bm2.bmHeight, buf2.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdc);
        return memcmp(buf1.data(), buf2.data(), cmpSize) == 0;
    }
    }
    return false;
}

bool ClipEntry::RestoreToClipboard(HWND owner) const {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();

    bool ok = false;
    switch (type) {
    case ClipType::Text: {
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* p = GlobalLock(hMem);
            if (p) {
                memcpy(p, text.c_str(), bytes);
                GlobalUnlock(hMem);
                ok = (SetClipboardData(CF_UNICODETEXT, hMem) != nullptr);
                if (!ok) GlobalFree(hMem);
            } else {
                GlobalFree(hMem);
            }
        }
        break;
    }
    case ClipType::Bitmap: {
        if (!hBitmap) break;
        BITMAP bm;
        GetObject(hBitmap, sizeof(bm), &bm);

        HDC hdcScreen = GetDC(nullptr);
        HDC hdcSrc = CreateCompatibleDC(hdcScreen);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcSrc, hBitmap);

        // Create a DIB for clipboard
        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(bi);
        bi.biWidth = bm.bmWidth;
        bi.biHeight = bm.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        int stride = ((bm.bmWidth * 32 + 31) / 32) * 4;
        size_t dataSize = stride * bm.bmHeight;

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + dataSize);
        if (hMem) {
            void* p = GlobalLock(hMem);
            if (p) {
                memcpy(p, &bi, sizeof(bi));
                GetDIBits(hdcSrc, hBitmap, 0, bm.bmHeight,
                    (BYTE*)p + sizeof(BITMAPINFOHEADER),
                    (BITMAPINFO*)p, DIB_RGB_COLORS);
                GlobalUnlock(hMem);
                ok = (SetClipboardData(CF_DIB, hMem) != nullptr);
                if (!ok) GlobalFree(hMem);
            } else {
                GlobalFree(hMem);
            }
        }

        SelectObject(hdcSrc, hOld);
        DeleteDC(hdcSrc);
        ReleaseDC(nullptr, hdcScreen);
        break;
    }
    case ClipType::Files: {
        if (files.empty()) break;
        // Calculate DROPFILES size
        size_t totalChars = 0;
        for (auto& f : files) totalChars += f.size() + 1;
        totalChars += 1; // double null terminator

        size_t bytes = sizeof(DROPFILES) + totalChars * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
        if (hMem) {
            DROPFILES* df = (DROPFILES*)GlobalLock(hMem);
            if (df) {
                df->pFiles = sizeof(DROPFILES);
                df->fWide = TRUE;
                wchar_t* dest = (wchar_t*)((BYTE*)df + sizeof(DROPFILES));
                for (auto& f : files) {
                    memcpy(dest, f.c_str(), (f.size() + 1) * sizeof(wchar_t));
                    dest += f.size() + 1;
                }
                *dest = L'\0';
                GlobalUnlock(hMem);
                ok = (SetClipboardData(CF_HDROP, hMem) != nullptr);
                if (!ok) GlobalFree(hMem);
            } else {
                GlobalFree(hMem);
            }
        }
        break;
    }
    }

    CloseClipboard();
    return ok;
}

// ---- History implementation ----

History::History() = default;
History::~History() = default;

void History::SetMaxSize(int maxSize) {
    maxSize_ = maxSize;
    TrimToSize();
}

bool History::CaptureFromClipboard(HWND hwnd, bool ignoreDuplicates) {
    if (!OpenClipboard(hwnd)) return false;

    auto entry = std::make_unique<ClipEntry>();
    bool captured = false;

    // Try text first
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            const wchar_t* text = (const wchar_t*)GlobalLock(hData);
            if (text) {
                entry->type = ClipType::Text;
                entry->text = text;
                GlobalUnlock(hData);
                captured = true;
            }
        }
    }
    // Try bitmap
    else if (IsClipboardFormatAvailable(CF_DIB)) {
        HANDLE hData = GetClipboardData(CF_DIB);
        if (hData) {
            BITMAPINFOHEADER* pbi = (BITMAPINFOHEADER*)GlobalLock(hData);
            if (pbi) {
                HDC hdcScreen = GetDC(nullptr);
                void* bits = nullptr;

                BITMAPINFOHEADER biNew = *pbi;
                biNew.biBitCount = 32;
                biNew.biCompression = BI_RGB;
                biNew.biSizeImage = 0;

                HBITMAP hbm = CreateDIBSection(hdcScreen, (BITMAPINFO*)&biNew,
                    DIB_RGB_COLORS, &bits, nullptr, 0);
                if (hbm && bits) {
                    // Copy bits from clipboard DIB
                    HDC hdcDst = CreateCompatibleDC(hdcScreen);
                    HBITMAP hOld = (HBITMAP)SelectObject(hdcDst, hbm);
                    int colors = 0;
                    if (pbi->biBitCount <= 8) colors = 1 << pbi->biBitCount;
                    void* srcBits = (BYTE*)pbi + pbi->biSize + colors * sizeof(RGBQUAD);
                    SetDIBitsToDevice(hdcDst, 0, 0, pbi->biWidth, abs(pbi->biHeight),
                        0, 0, 0, abs(pbi->biHeight), srcBits, (BITMAPINFO*)pbi, DIB_RGB_COLORS);
                    SelectObject(hdcDst, hOld);
                    DeleteDC(hdcDst);

                    entry->type = ClipType::Bitmap;
                    entry->hBitmap = hbm;
                    captured = true;
                } else if (hbm) {
                    DeleteObject(hbm);
                }
                ReleaseDC(nullptr, hdcScreen);
                GlobalUnlock(hData);
            }
        }
    }
    // Try file drop
    else if (IsClipboardFormatAvailable(CF_HDROP)) {
        HANDLE hData = GetClipboardData(CF_HDROP);
        if (hData) {
            HDROP hDrop = (HDROP)hData;
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            if (count > 0) {
                entry->type = ClipType::Files;
                for (UINT i = 0; i < count; i++) {
                    wchar_t path[MAX_PATH];
                    if (DragQueryFileW(hDrop, i, path, MAX_PATH)) {
                        entry->files.push_back(path);
                    }
                }
                // Build display text
                entry->text = L"";
                for (auto& f : entry->files) {
                    if (!entry->text.empty()) entry->text += L"\r\n";
                    entry->text += f;
                }
                captured = true;
            }
        }
    }

    CloseClipboard();

    if (!captured) return false;

    // Check duplicates
    if (ignoreDuplicates && !entries_.empty()) {
        if (entry->Matches(*entries_.front())) {
            return false; // Same as current top entry
        }
        // Remove any existing duplicate and move to front
        for (auto it = entries_.begin() + 1; it != entries_.end(); ++it) {
            if (entry->Matches(**it)) {
                entries_.erase(it);
                break;
            }
        }
    }

    entries_.push_front(std::move(entry));
    TrimToSize();
    return true;
}

void History::RemoveEntry(int index) {
    if (index >= 0 && index < (int)entries_.size()) {
        entries_.erase(entries_.begin() + index);
    }
}

void History::Clear() {
    entries_.clear();
}

void History::ClearBitmaps() {
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [](const auto& e) { return e->type == ClipType::Bitmap; }),
        entries_.end());
}

void History::UpdateEntryText(int index, const std::wstring& newText) {
    if (index >= 0 && index < (int)entries_.size()) {
        if (entries_[index]->type == ClipType::Text) {
            entries_[index]->text = newText;
        }
    }
}

std::vector<int> History::Search(const std::wstring& query) const {
    std::vector<int> results;
    if (query.empty()) return results;

    // Case-insensitive search
    std::wstring queryLower = query;
    std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::towlower);

    for (int i = 0; i < (int)entries_.size(); i++) {
        std::wstring text = entries_[i]->text;
        std::transform(text.begin(), text.end(), text.begin(), ::towlower);
        if (text.find(queryLower) != std::wstring::npos) {
            results.push_back(i);
        }
    }
    return results;
}

void History::TrimToSize() {
    while ((int)entries_.size() > maxSize_) {
        entries_.pop_back();
    }
}

// ---- Persistence ----
// Format: magic(4) version(4) count(4) [ type(4) textLen(4) text(bytes) bitmapDataLen(4) bitmapData(bytes) timestamp(8) ]

static const DWORD kMagic = 0x50494C43; // "CLIP"
static const DWORD kVersion = 1;

bool History::SaveToFile(const std::wstring& path, bool purgeBitmaps) const {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written;
    auto writeU32 = [&](DWORD v) {
        WriteFile(hFile, &v, 4, &written, nullptr);
    };
    auto writeBytes = [&](const void* data, DWORD size) {
        WriteFile(hFile, data, size, &written, nullptr);
    };

    writeU32(kMagic);
    writeU32(kVersion);
    writeU32((DWORD)entries_.size());

    for (auto& entry : entries_) {
        writeU32((DWORD)entry->type);
        // Text
        DWORD textLen = (DWORD)entry->text.size();
        writeU32(textLen);
        if (textLen > 0) {
            writeBytes(entry->text.c_str(), textLen * sizeof(wchar_t));
        }
        // Bitmap
        if (!purgeBitmaps && entry->type == ClipType::Bitmap && entry->hBitmap) {
            BITMAP bm;
            GetObject(entry->hBitmap, sizeof(bm), &bm);

            BITMAPINFOHEADER bi = {};
            bi.biSize = sizeof(bi);
            bi.biWidth = bm.bmWidth;
            bi.biHeight = bm.bmHeight;
            bi.biPlanes = 1;
            bi.biBitCount = 32;
            bi.biCompression = BI_RGB;

            int stride = ((bm.bmWidth * 32 + 31) / 32) * 4;
            DWORD dataSize = stride * bm.bmHeight;

            std::vector<BYTE> pixels(dataSize);
            HDC hdc = GetDC(nullptr);
            GetDIBits(hdc, entry->hBitmap, 0, bm.bmHeight, pixels.data(),
                (BITMAPINFO*)&bi, DIB_RGB_COLORS);
            ReleaseDC(nullptr, hdc);

            DWORD totalSize = (DWORD)(sizeof(bi) + dataSize);
            writeU32(totalSize);
            writeBytes(&bi, sizeof(bi));
            writeBytes(pixels.data(), dataSize);
        } else {
            writeU32(0); // No bitmap data
        }
        // Files
        DWORD fileCount = (DWORD)entry->files.size();
        writeU32(fileCount);
        for (auto& f : entry->files) {
            DWORD fLen = (DWORD)f.size();
            writeU32(fLen);
            if (fLen > 0) writeBytes(f.c_str(), fLen * sizeof(wchar_t));
        }
        // Timestamp
        writeBytes(&entry->timestamp, sizeof(FILETIME));
    }

    CloseHandle(hFile);
    return true;
}

bool History::LoadFromFile(const std::wstring& path) {
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
    if (magic != kMagic || version != kVersion) {
        CloseHandle(hFile);
        return false;
    }

    DWORD count = readU32();
    if (count > 10000) { CloseHandle(hFile); return false; } // Sanity check

    entries_.clear();
    bool readOk = true;

    for (DWORD i = 0; i < count && readOk; i++) {
        auto entry = std::make_unique<ClipEntry>();
        entry->type = (ClipType)readU32();

        // Text
        DWORD textLen = readU32();
        if (textLen > 0 && textLen < 10000000) {
            entry->text.resize(textLen);
            if (!readBytes(entry->text.data(), textLen * sizeof(wchar_t))) { readOk = false; break; }
        }

        // Bitmap
        DWORD bmpDataSize = readU32();
        if (bmpDataSize > 0 && bmpDataSize < 100000000) {
            std::vector<BYTE> bmpData(bmpDataSize);
            if (!readBytes(bmpData.data(), bmpDataSize)) { readOk = false; break; }

            if (bmpDataSize >= sizeof(BITMAPINFOHEADER)) {
                BITMAPINFOHEADER* pbi = (BITMAPINFOHEADER*)bmpData.data();
                void* bits = nullptr;
                HDC hdc = GetDC(nullptr);
                HBITMAP hbm = CreateDIBSection(hdc, (BITMAPINFO*)pbi,
                    DIB_RGB_COLORS, &bits, nullptr, 0);
                if (hbm && bits) {
                    DWORD pixelOffset = (DWORD)pbi->biSize;
                    DWORD pixelSize = bmpDataSize - pixelOffset;
                    memcpy(bits, bmpData.data() + pixelOffset, pixelSize);
                    entry->hBitmap = hbm;
                } else if (hbm) {
                    DeleteObject(hbm);
                }
                ReleaseDC(nullptr, hdc);
            }
        }

        // Files
        DWORD fileCount = readU32();
        if (fileCount > 10000) { readOk = false; break; }
        for (DWORD f = 0; f < fileCount; f++) {
            DWORD fLen = readU32();
            if (fLen > 0 && fLen < 10000000) {
                std::wstring file(fLen, L'\0');
                if (!readBytes(file.data(), fLen * sizeof(wchar_t))) { readOk = false; break; }
                entry->files.push_back(std::move(file));
            }
        }
        if (!readOk) break;

        // Timestamp
        if (!readBytes(&entry->timestamp, sizeof(FILETIME))) { readOk = false; break; }

        entries_.push_back(std::move(entry));
    }

    CloseHandle(hFile);

    if (!readOk) {
        entries_.clear();
        return false;
    }

    TrimToSize();
    return true;
}
