#pragma once

#include <compare>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <string>

namespace swiftlist {

// 128-bit File Reference Number (NTFS uses low 64 bits, ReFS uses full 128)
struct UInt128 {
    uint64_t low = 0;
    uint64_t high = 0;

    constexpr UInt128() = default;
    constexpr UInt128(uint64_t l, uint64_t h) : low(l), high(h) {}

    // Comparison (defaulted = strong_ordering on both fields)
    auto operator<=>(const UInt128&) const = default;
    bool operator==(const UInt128&) const = default;

    // True if the high 64 bits are zero (NTFS-compatible FRN)
    [[nodiscard]] constexpr bool Is64Bit() const noexcept { return high == 0; }

    [[nodiscard]] constexpr uint64_t AsUint64() const noexcept { return low; }
};

// Drive letter abstraction (e.g. 'C' for C:)
struct DriveLetter {
    wchar_t letter = L'\0';

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return (letter >= L'A' && letter <= L'Z') || (letter >= L'a' && letter <= L'z');
    }

    [[nodiscard]] wchar_t Upper() const noexcept {
        return (letter >= L'a' && letter <= L'z') ? (letter - L'a' + L'A') : letter;
    }

    // e.g. L"C:\\"
    [[nodiscard]] std::wstring RootPath() const {
        return std::wstring(1, Upper()) + L":\\";
    }

    // e.g. L"\\\\.\\C:"
    [[nodiscard]] std::wstring DosDevicePath() const {
        return std::wstring(L"\\\\.\\") + Upper() + L":";
    }
};

// File attribute flags (matching FILE_ATTRIBUTE_* constants)
enum class FileAttributes : uint32_t {
    None               = 0,
    ReadOnly           = 0x00000001,
    Hidden             = 0x00000002,
    System             = 0x00000004,
    Directory          = 0x00000010,
    Archive            = 0x00000020,
    Device             = 0x00000040,
    Normal             = 0x00000080,
    Temporary          = 0x00000100,
    SparseFile         = 0x00000200,
    ReparsePoint       = 0x00000400,
    Compressed         = 0x00000800,
    Offline            = 0x00001000,
    NotContentIndexed  = 0x00002000,
    Encrypted          = 0x00004000,
    IntegrityStream    = 0x00008000,
    Virtual            = 0x00010000,
    NoScrubData        = 0x00020000,
    ExtendedAttributes = 0x00040000,
};

constexpr FileAttributes operator|(FileAttributes a, FileAttributes b) {
    return static_cast<FileAttributes>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr FileAttributes operator&(FileAttributes a, FileAttributes b) {
    return static_cast<FileAttributes>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr FileAttributes& operator|=(FileAttributes& a, FileAttributes b) {
    a = a | b;
    return a;
}

[[nodiscard]] constexpr bool HasFlag(FileAttributes value, FileAttributes flag) noexcept {
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

}  // namespace swiftlist

// std::hash<UInt128> specialization
template <>
struct std::hash<swiftlist::UInt128> {
    [[nodiscard]] size_t operator()(const swiftlist::UInt128& v) const noexcept {
        // Boost-style hash_combine
        size_t h1 = std::hash<uint64_t>{}(v.low);
        size_t h2 = std::hash<uint64_t>{}(v.high);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
