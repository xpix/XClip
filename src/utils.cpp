#include "utils.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>

namespace Utils {

std::wstring Truncate(const std::wstring& str, size_t maxLen) {
    if (str.size() <= maxLen) return str;
    return str.substr(0, maxLen) + L"...";
}

std::wstring SingleLine(const std::wstring& str) {
    std::wstring result;
    result.reserve(str.size());
    for (wchar_t c : str) {
        if (c == L'\r') continue;
        if (c == L'\n') { result += L' '; continue; }
        if (c == L'\t') { result += L' '; continue; }
        result += c;
    }
    return result;
}

std::wstring GetAppDataPath() {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        std::wstring dir = std::wstring(path) + L"\\XClip";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return L".";
}

std::wstring GetExePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

std::wstring EscapeAmpersand(const std::wstring& str) {
    std::wstring result;
    result.reserve(str.size() + 8);
    for (wchar_t c : str) {
        if (c == L'&') result += L"&&";
        else result += c;
    }
    return result;
}

std::wstring FormatTime(const FILETIME& ft) {
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, nullptr);
    FILETIME localFt;
    FileTimeToLocalFileTime(&ft, &localFt);
    FileTimeToSystemTime(&localFt, &st);
    wchar_t buf[64];
    wsprintfW(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

HBITMAP CreateThumbnail(HBITMAP hbm, int maxWidth, int maxHeight) {
    if (!hbm) return nullptr;

    BITMAP bm;
    GetObject(hbm, sizeof(bm), &bm);

    double scaleX = (double)maxWidth / bm.bmWidth;
    double scaleY = (double)maxHeight / bm.bmHeight;
    double scale = (std::min)(scaleX, scaleY);
    if (scale >= 1.0) scale = 1.0;

    int newW = (int)(bm.bmWidth * scale);
    int newH = (int)(bm.bmHeight * scale);
    if (newW < 1) newW = 1;
    if (newH < 1) newH = 1;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HDC hdcDst = CreateCompatibleDC(hdcScreen);

    HBITMAP hbmThumb = CreateCompatibleBitmap(hdcScreen, newW, newH);
    HBITMAP hbmOldSrc = (HBITMAP)SelectObject(hdcSrc, hbm);
    HBITMAP hbmOldDst = (HBITMAP)SelectObject(hdcDst, hbmThumb);

    SetStretchBltMode(hdcDst, HALFTONE);
    StretchBlt(hdcDst, 0, 0, newW, newH, hdcSrc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

    SelectObject(hdcSrc, hbmOldSrc);
    SelectObject(hdcDst, hbmOldDst);
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    ReleaseDC(nullptr, hdcScreen);

    return hbmThumb;
}

SIZE GetBitmapSize(HBITMAP hbm) {
    BITMAP bm = {};
    if (hbm) GetObject(hbm, sizeof(bm), &bm);
    return { bm.bmWidth, bm.bmHeight };
}

} // namespace Utils
