#pragma once

#include "libengine/common/defs.h"

#include <cstdint>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace swiftlist::indexer {

struct VolumeIdentity {
    std::string FileSystemType; // "NTFS", "ReFS", "FAT32", etc.
    uint32_t SerialNumber = 0;
    bool IsNtfs = false;
    bool IsReFs = false;
};

// RAII wrapper around a volume handle.
class VolumeHandle {
public:
    VolumeHandle() = default;
    ~VolumeHandle();

    VolumeHandle(const VolumeHandle&) = delete;
    VolumeHandle& operator=(const VolumeHandle&) = delete;
    VolumeHandle(VolumeHandle&& other) noexcept;
    VolumeHandle& operator=(VolumeHandle&& other) noexcept;

    // Opens a volume for IOCTL operations.
    // path should be in the form "\\.\C:"
    bool Open(const std::wstring& path);

    // Opens a volume by drive letter.
    bool Open(wchar_t driveLetter);

    void Close() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept { return m_handle != INVALID_HANDLE_VALUE; }
    [[nodiscard]] HANDLE Get() const noexcept { return m_handle; }

private:
#ifdef _WIN32
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
    void* m_handle = nullptr;
#endif
};

// Gets the filesystem type and serial number for a drive.
// Returns false on failure.
bool GetVolumeIdentity(wchar_t driveLetter, VolumeIdentity& identity);

// Gets the root directory FRN for a drive.
// Returns true on success.
bool GetRootFrn(wchar_t driveLetter, UInt128& rootFrn);

// Determines if USN journal is supported (NTFS or ReFS).
inline bool SupportsUsnJournal(const VolumeIdentity& identity) {
    return identity.IsNtfs || identity.IsReFs;
}

} // namespace swiftlist::indexer
