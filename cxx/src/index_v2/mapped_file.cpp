#include "index_v2/mapped_file.h"

#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace swiftlist::index_v2 {

MemoryMappedFile::~MemoryMappedFile() { Close(); }

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
    : m_data(other.m_data), m_size(other.m_size) {
#ifdef _WIN32
    m_file = other.m_file;
    m_mapping = other.m_mapping;
    other.m_file = nullptr;
    other.m_mapping = nullptr;
#endif
    other.m_data = nullptr;
    other.m_size = 0;
}

MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept {
    if (this != &other) {
        Close();
#ifdef _WIN32
        m_file = other.m_file;
        m_mapping = other.m_mapping;
        other.m_file = nullptr;
        other.m_mapping = nullptr;
#endif
        m_data = other.m_data;
        m_size = other.m_size;
        other.m_data = nullptr;
        other.m_size = 0;
    }
    return *this;
}

void MemoryMappedFile::Open(const std::string& path) {
    Close();
#ifdef _WIN32
    // Must allow DELETE share so SnapshotWriter can File.Replace the file.
    HANDLE hFile = ::CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open snapshot file: " + path);

    LARGE_INTEGER fileSize;
    if (!::GetFileSizeEx(hFile, &fileSize)) {
        ::CloseHandle(hFile);
        throw std::runtime_error("Cannot get file size: " + path);
    }

    HANDLE hMapping = ::CreateFileMappingW(
        hFile,
        nullptr,
        PAGE_READONLY,
        0, 0,
        nullptr);
    if (!hMapping) {
        ::CloseHandle(hFile);
        throw std::runtime_error("Cannot create file mapping: " + path);
    }

    void* data = ::MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!data) {
        ::CloseHandle(hMapping);
        ::CloseHandle(hFile);
        throw std::runtime_error("Cannot map view of file: " + path);
    }

    m_file = hFile;
    m_mapping = hMapping;
    m_data = data;
    m_size = static_cast<size_t>(fileSize.QuadPart);
#else
    (void)path;
    throw std::runtime_error("MemoryMappedFile::Open is Windows-only");
#endif
}

void MemoryMappedFile::Close() noexcept {
#ifdef _WIN32
    if (m_data) {
        ::UnmapViewOfFile(m_data);
        m_data = nullptr;
    }
    if (m_mapping) {
        ::CloseHandle(m_mapping);
        m_mapping = nullptr;
    }
    if (m_file) {
        ::CloseHandle(m_file);
        m_file = nullptr;
    }
#endif
    m_size = 0;
}

} // namespace swiftlist::index_v2
