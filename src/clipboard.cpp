#include "clipboard.h"

bool ClipboardMonitor::Start(HWND hwnd) {
    hwnd_ = hwnd;
    started_ = AddClipboardFormatListener(hwnd);
    return started_;
}

void ClipboardMonitor::Stop() {
    if (started_ && hwnd_) {
        RemoveClipboardFormatListener(hwnd_);
        started_ = false;
    }
}

void ClipboardMonitor::OnClipboardUpdate() {
    if (ignoreNext_) {
        ignoreNext_ = false;
    }
}
