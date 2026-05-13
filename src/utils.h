#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace Utils {

// Truncate a wide string to maxLen characters, appending "..." if truncated
std::wstring Truncate(const std::wstring& str, size_t maxLen);

// Replace newlines with spaces for single-line display
std::wstring SingleLine(const std::wstring& str);

// Get the %APPDATA%\XClip directory, creates it if needed
std::wstring GetAppDataPath();

// Get the path to the executable
std::wstring GetExePath();

// Escape '&' for menu display
std::wstring EscapeAmpersand(const std::wstring& str);

// Format a FILETIME as a readable string
std::wstring FormatTime(const FILETIME& ft);

// Create a thumbnail of a bitmap (returns new HBITMAP, caller must delete)
HBITMAP CreateThumbnail(HBITMAP hbm, int maxWidth, int maxHeight);

// Get bitmap dimensions
SIZE GetBitmapSize(HBITMAP hbm);

} // namespace Utils
