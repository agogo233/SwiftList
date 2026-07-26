#pragma once

#include <string>
#include <string_view>

namespace swiftlist {

// Convert a UTF-8 byte span to a UTF-16 wide string (Win32 native encoding)
[[nodiscard]] std::wstring Utf8ToWide(std::string_view utf8);

// Convert a UTF-16 wide string view to a UTF-8 byte string
[[nodiscard]] std::string WideToUtf8(std::wstring_view wide);

// Check if a byte sequence is valid UTF-8
[[nodiscard]] bool IsValidUtf8(std::string_view utf8);

// Fast ASCII-only path: convert ASCII chars to lower in-place
void AsciiToLowerInPlace(std::string& s);

// Case-insensitive ASCII string comparison (fast path for file names)
[[nodiscard]] bool AsciiCaseInsensitiveEquals(std::string_view a, std::string_view b);

}  // namespace swiftlist
