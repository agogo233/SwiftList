#include "indexer/volume_helper.h"

#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace swiftlist::indexer {

// --- VolumeHandle ---

VolumeHandle::~VolumeHandle() { Close(); }

VolumeHandle::VolumeHandle(VolumeHandle&& other) noexcept {
#ifdef _WIN32
    m_handle = other.m_handle;
    other.m_handle = INVALID_HANDLE_VALUE;
#else
    m_handle = other.m_handle;
    other.m_handle = nullptr;
#endif
}

VolumeHandle& VolumeHandle::operator=(VolumeHandle&& other) noexcept {
    if (this != &other) {
        Close();
#ifdef _WIN32
        m_handle = other.m_handle;
        other.m_handle = INVALID_HANDLE_VALUE;
#else
        m_handle = other.m_handle;
        other.m_handle = nullptr;
#endif
    }
    return *this;
}

bool VolumeHandle::Open(const std::wstring& path) {
    Close();
#ifdef _WIN32
    m_handle = ::CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    return m_handle != INVALID_HANDLE_VALUE;
#else
    (void)path;
    return false;
#endif
}

bool VolumeHandle::Open(wchar_t driveLetter) {
    // Build path: L"\\\\.\\C:"
    std::wstring path = L"\\\\.\\";
    path += driveLetter;
    path += L':';
    return Open(path);
}

void VolumeHandle::Close() noexcept {
#ifdef _WIN32
    if (m_handle != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
#endif
}

// --- Free functions ---

bool GetVolumeIdentity(wchar_t driveLetter, VolumeIdentity& identity) {
#ifdef _WIN32
    // Open root path "C:\".
    std::wstring rootPath;
    if (driveLetter >= L'a' && driveLetter <= L'z') {
        rootPath += static_cast<wchar_t>(driveLetter - L'a' + L'A');
    } else {
        rootPath += driveLetter;
    }
    rootPath += L":\\";

    wchar_t fsName[16] = {};
    DWORD serial = 0;
    DWORD maxCompLen = 0;
    DWORD fsFlags = 0;

    BOOL ok = ::GetVolumeInformationW(
        rootPath.c_str(),
        nullptr, 0,          // volume name not needed
        &serial,
        &maxCompLen,
        &fsFlags,
        fsName,
        static_cast<DWORD>(std::size(fsName)));

    if (!ok) return false;

    // fsName is ASCII (NTFS/ReFS/etc), convert wchar_t to char.
    for (size_t i = 0; i < std::size(fsName) && fsName[i] != L'\0'; ++i) {
        identity.FileSystemType.push_back(static_cast<char>(fsName[i]));
    }
    identity.SerialNumber = serial;
    identity.IsNtfs = (identity.FileSystemType == "NTFS");
    identity.IsReFs = (identity.FileSystemType == "ReFS");
    return true;
#else
    (void)driveLetter;
    (void)identity;
    return false;
#endif
}

bool GetRootFrn(wchar_t driveLetter, UInt128& rootFrn) {
#ifdef _WIN32
    // Open root path.
    std::wstring rootPath;
    if (driveLetter >= L'a' && driveLetter <= L'z') {
        rootPath += static_cast<wchar_t>(driveLetter - L'a' + L'A');
    } else {
        rootPath += driveLetter;
    }
    rootPath += L":\\";

    HANDLE hFile = ::CreateFileW(
        rootPath.c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) return false;

    // Try FileIdExtdDirectoryInfo (18) for 128-bit FRN.
    struct FileIdExtDirInfo {
        ULONG NextEntryOffset;
        ULONG FileIndex;
        LARGE_INTEGER CreationTime;
        LARGE_INTEGER LastAccessTime;
        LARGE_INTEGER LastWriteTime;
        LARGE_INTEGER ChangeTime;
        LARGE_INTEGER EndOfFile;
        LARGE_INTEGER AllocationSize;
        ULONG FileAttributes;
        ULONG FileNameLength;
        ULONG EaSize;
        ULONG ReparsePointTag;
        ULONGLONG FileId;
    };

    FileIdExtDirInfo info{};
    BOOL ok = ::GetFileInformationByHandleEx(
        hFile,
        static_cast<FILE_INFO_BY_HANDLE_CLASS>(18), // FileIdExtdDirectoryInfo
        &info,
        sizeof(info));

    if (ok) {
        rootFrn.low = info.FileId;
        rootFrn.high = 0;
    } else {
        // Fallback: GetFileInformationByHandle gives 64-bit index.
        BY_HANDLE_FILE_INFORMATION fi{};
        ok = ::GetFileInformationByHandle(hFile, &fi);
        if (ok) {
            rootFrn.low = (static_cast<uint64_t>(fi.nFileIndexHigh) << 32) | fi.nFileIndexLow;
            rootFrn.high = 0;
        }
    }

    ::CloseHandle(hFile);
    return ok;
#else
    (void)driveLetter;
    (void)rootFrn;
    return false;
#endif
}

} // namespace swiftlist::indexer
