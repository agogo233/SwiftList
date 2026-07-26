#pragma once

#include <cstddef>
#include <string>

namespace swiftlist::index_v2 {

// RAII wrapper around a Win32 memory-mapped file for read-only access.
// Split out from Snapshot purely to keep each file under the repo's
// per-file line limit; this class has no index-level logic.
class MemoryMappedFile {
public:
    MemoryMappedFile() = default;
    ~MemoryMappedFile();

    // Non-copyable, movable.
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
    MemoryMappedFile(MemoryMappedFile&& other) noexcept;
    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;

    // Opens and maps a file for reading.
    void Open(const std::string& path);

    // Returns a pointer to the mapped data, or nullptr if not open.
    [[nodiscard]] const void* Data() const noexcept { return m_data; }
    [[nodiscard]] size_t Size() const noexcept { return m_size; }
    [[nodiscard]] bool IsOpen() const noexcept { return m_data != nullptr; }

private:
    void Close() noexcept;

#ifdef _WIN32
    void* m_file = nullptr;     // HANDLE
    void* m_mapping = nullptr;  // HANDLE
#endif
    void* m_data = nullptr;
    size_t m_size = 0;
};

} // namespace swiftlist::index_v2
