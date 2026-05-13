#pragma once
#include <windows.h>

class ClipboardMonitor {
public:
    bool Start(HWND hwnd);
    void Stop();
    bool IsStarted() const { return started_; }

    // Call in WndProc when WM_CLIPBOARDUPDATE is received
    // Sets 'selfTriggered' to true if we triggered the clipboard change ourselves
    void OnClipboardUpdate();

    // Set flag to ignore the next clipboard change (when we paste from history)
    void SetIgnoreNext(bool ignore) { ignoreNext_ = ignore; }
    bool ShouldIgnore() const { return ignoreNext_; }

private:
    HWND hwnd_ = nullptr;
    bool started_ = false;
    bool ignoreNext_ = false;
};
