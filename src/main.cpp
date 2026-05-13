#include <windows.h>
#include <objbase.h>
#include "app.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    // Write debug log to help diagnose startup issues
    auto logDebug = [](const wchar_t* msg) {
        HANDLE hFile = CreateFileW(L"C:\\Users\\D063239\\Desktop\\XClip\\debug.log",
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hFile, msg, (DWORD)(wcslen(msg) * sizeof(wchar_t)), &written, nullptr);
            WriteFile(hFile, L"\r\n", 4, &written, nullptr);
            CloseHandle(hFile);
        }
    };

    logDebug(L"XClip starting...");

    // Single instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"XClip_SingleInstance_Mutex");
    DWORD mutexErr = GetLastError();
    if (mutexErr == ERROR_ALREADY_EXISTS) {
        logDebug(L"Another instance already running.");
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }
    logDebug(L"Mutex acquired.");

    logDebug(L"COM initialized.");

    App& app = App::Instance();

    logDebug(L"Calling Init...");
    if (!app.Init(hInstance)) {
        logDebug(L"Init FAILED!");
        MessageBoxW(nullptr, L"Failed to initialize XClip.", L"XClip Error", MB_OK | MB_ICONERROR);
        if (hMutex) {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        CoUninitialize();
        return 1;
    }

    logDebug(L"Init succeeded. Entering message loop...");
    int result = app.Run();

    logDebug(L"Message loop ended. Shutting down...");
    app.Shutdown();

    CoUninitialize();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return result;
}
