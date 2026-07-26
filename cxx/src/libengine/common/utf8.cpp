#include "libengine/common/utf8.h"

#include <Windows.h>

#include <algorithm>

namespace swiftlist {

std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};

    const int utf8Len = static_cast<int>(utf8.size());
    const int wideLen = ::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8.data(), utf8Len,
        nullptr, 0);

    if (wideLen == 0) return {};

    std::wstring result(wideLen, L'\0');
    ::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8.data(), utf8Len,
        result.data(), wideLen);

    return result;
}

std::string WideToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};

    const int wideLen = static_cast<int>(wide.size());
    const int utf8Len = ::WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        wide.data(), wideLen,
        nullptr, 0,
        nullptr, nullptr);

    if (utf8Len == 0) return {};

    std::string result(utf8Len, '\0');
    ::WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        wide.data(), wideLen,
        result.data(), utf8Len,
        nullptr, nullptr);

    return result;
}

bool IsValidUtf8(std::string_view utf8) {
    if (utf8.empty()) return true;

    const int result = ::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0);

    return result != 0;
}

void AsciiToLowerInPlace(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c; });
}

bool AsciiCaseInsensitiveEquals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        // Fast ASCII case-fold
        if (ca >= 'A' && ca <= 'Z') ca = ca - 'A' + 'a';
        if (cb >= 'A' && cb <= 'Z') cb = cb - 'A' + 'a';
        if (ca != cb) return false;
    }
    return true;
}

}  // namespace swiftlist
