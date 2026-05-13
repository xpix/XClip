#include "hotkey.h"

bool HotkeyManager::Register(HWND hwnd, int id, UINT modifiers, UINT vk) {
    if (count_ >= kMaxHotkeys) return false;

    if (RegisterHotKey(hwnd, id, modifiers, vk)) {
        keys_[count_++] = { id, modifiers, vk };
        return true;
    }
    return false;
}

void HotkeyManager::Unregister(HWND hwnd, int id) {
    UnregisterHotKey(hwnd, id);
    for (int i = 0; i < count_; i++) {
        if (keys_[i].id == id) {
            keys_[i] = keys_[count_ - 1];
            count_--;
            break;
        }
    }
}

void HotkeyManager::UnregisterAll(HWND hwnd) {
    for (int i = 0; i < count_; i++) {
        UnregisterHotKey(hwnd, keys_[i].id);
    }
    count_ = 0;
}
