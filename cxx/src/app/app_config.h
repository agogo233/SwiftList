#pragma once

#include <string>
#include <vector>

namespace swiftlist::app {

struct AppConfig {
    int HotkeyModifiers = 2;    // MOD_CONTROL
    int HotkeyKey = 32;         // VK_SPACE
    std::wstring Theme = L"Dark";
    std::vector<wchar_t> IndexedDrives;

    static AppConfig Load(const std::wstring& path);
    bool Save(const std::wstring& path) const;
};

} // namespace swiftlist::app
