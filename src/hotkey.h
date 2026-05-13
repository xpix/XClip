#pragma once
#include <windows.h>

class HotkeyManager {
public:
    bool Register(HWND hwnd, int id, UINT modifiers, UINT vk);
    void Unregister(HWND hwnd, int id);
    void UnregisterAll(HWND hwnd);

private:
    struct RegisteredKey {
        int id;
        UINT modifiers;
        UINT vk;
    };
    static constexpr int kMaxHotkeys = 8;
    RegisteredKey keys_[kMaxHotkeys] = {};
    int count_ = 0;
};
