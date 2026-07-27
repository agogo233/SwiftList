#include "app/app_config.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>

namespace swiftlist::app {

namespace {

constexpr const wchar_t* kSection = L"General";
constexpr const wchar_t* kKeyModifiers = L"HotkeyModifiers";
constexpr const wchar_t* kKeyKey = L"HotkeyKey";
constexpr const wchar_t* kKeyTheme = L"Theme";
constexpr const wchar_t* kKeyDrives = L"IndexedDrives";

void WritePrivateProfileIntW(const wchar_t* section, const wchar_t* key,
                               int value, const wchar_t* file) {
    wchar_t buf[32] = {};
    _snwprintf_s(buf, _TRUNCATE, L"%d", value);
    WritePrivateProfileStringW(section, key, buf, file);
}

} // namespace

AppConfig AppConfig::Load(const std::wstring& path) {
    AppConfig cfg;

    wchar_t buffer[1024] = {};

    cfg.HotkeyModifiers = GetPrivateProfileIntW(kSection, kKeyModifiers,
                                                  cfg.HotkeyModifiers, path.c_str());
    cfg.HotkeyKey = GetPrivateProfileIntW(kSection, kKeyKey,
                                            cfg.HotkeyKey, path.c_str());

    GetPrivateProfileStringW(kSection, kKeyTheme, cfg.Theme.c_str(),
                              buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
    cfg.Theme = buffer;

    GetPrivateProfileStringW(kSection, kKeyDrives, L"",
                              buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
    for (const wchar_t* p = buffer; *p; ++p) {
        if (*p >= L'A' && *p <= L'Z') {
            cfg.IndexedDrives.push_back(*p);
        }
    }

    return cfg;
}

bool AppConfig::Save(const std::wstring& path) const {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    if (ec) return false;

    if (!WritePrivateProfileIntW(kSection, kKeyModifiers, HotkeyModifiers, path.c_str())) return false;
    if (!WritePrivateProfileIntW(kSection, kKeyKey, HotkeyKey, path.c_str())) return false;
    if (!WritePrivateProfileStringW(kSection, kKeyTheme, Theme.c_str(), path.c_str())) return false;

    std::wstring drives;
    for (wchar_t d : IndexedDrives) {
        drives.push_back(d);
        drives.push_back(L',');
    }
    if (!drives.empty()) drives.pop_back();

    if (!WritePrivateProfileStringW(kSection, kKeyDrives, drives.c_str(), path.c_str())) return false;
    return true;
}

} // namespace swiftlist::app
