#pragma once

#include "index_v2/snapshot_format.h"
#include "index_v2/mapped_file.h"

#include <memory>
#include <span>
#include <string>

namespace swiftlist::index_v2 {

// Read-only view of an IndexV2 snapshot.
class Snapshot {
public:
    Snapshot();
    ~Snapshot();

    // Non-copyable, movable.
    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    Snapshot(Snapshot&&) noexcept;
    Snapshot& operator=(Snapshot&&) noexcept;

    // Opens and maps the snapshot file.
    void Open(const std::string& path);

    // Closes the snapshot and unmaps.
    void Close() noexcept;

    [[nodiscard]] bool IsOpen() const noexcept { return m_base != nullptr; }

    // Header metadata.
    [[nodiscard]] const Meta& GetMeta() const { return m_meta; }

    // Column accessors (valid while the Snapshot is open).
    [[nodiscard]] std::span<const uint32_t> NameIds() const;
    [[nodiscard]] std::span<const uint16_t> Flags() const;
    [[nodiscard]] std::span<const int32_t> ParentIndexes() const;
    [[nodiscard]] std::span<const uint64_t> UniqueMasks() const;
    [[nodiscard]] std::span<const uint32_t> NameOffsets() const;
    [[nodiscard]] std::span<const uint8_t> NameBlob() const;
    [[nodiscard]] std::span<const UInt128> Ids() const;
    [[nodiscard]] std::span<const int64_t> Sizes() const;
    [[nodiscard]] std::span<const uint32_t> CreationTimes() const;
    [[nodiscard]] std::span<const uint32_t> LastWriteTimes() const;
    [[nodiscard]] std::span<const uint32_t> LastAccessTimes() const;
    [[nodiscard]] std::span<const int32_t> ChildStarts() const;
    [[nodiscard]] std::span<const int32_t> Children() const;
    [[nodiscard]] std::span<const int32_t> UidStarts() const;
    [[nodiscard]] std::span<const int32_t> UidRows() const;
    [[nodiscard]] std::span<const int32_t> OrphanRows() const;
    [[nodiscard]] std::span<const UInt128> OrphanFrns() const;
    [[nodiscard]] std::span<const uint64_t> UniqueAsciiBits() const;

    [[nodiscard]] int RowCount() const { return m_meta.RowCount; }

    // High-level accessors.
    [[nodiscard]] std::span<const uint8_t> UniqueNameUtf8(int uid) const;
    [[nodiscard]] std::string GetName(int row) const;
    [[nodiscard]] bool IsDirectory(int row) const;
    [[nodiscard]] bool IsDeleted(int row) const;
    [[nodiscard]] bool IsUniqueAscii(int uid) const;
    [[nodiscard]] std::span<const int32_t> ChildrenOf(int row) const;
    [[nodiscard]] int FindRowById(UInt128 id) const;
    [[nodiscard]] bool TryGetOrphanParent(int row, UInt128& parentFrn) const;

private:
    const std::byte* SectionPointer(int sectionIndex) const;

    MemoryMappedFile m_mapped;
    Meta m_meta;
    const std::byte* m_base = nullptr;
    int64_t m_offsets[static_cast<int>(SnapshotSection::Count)];
};

} // namespace swiftlist::index_v2
