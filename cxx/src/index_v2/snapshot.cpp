#include "index_v2/snapshot.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>

namespace swiftlist::index_v2 {

// --- Snapshot ---

Snapshot::Snapshot() = default;
Snapshot::~Snapshot() { Close(); }

Snapshot::Snapshot(Snapshot&&) noexcept = default;
Snapshot& Snapshot::operator=(Snapshot&&) noexcept = default;

void Snapshot::Open(const std::string& path) {
    m_mapped.Open(path);
    m_base = static_cast<const std::byte*>(m_mapped.Data());

    // Read header.
    // We copy into a temp buffer since istream abstraction isn't needed here.
    // Use a simple memory reader.
    struct MemReader {
        const std::byte* ptr;
        size_t remaining;

        void Read(void* out, size_t len) {
            if (len > remaining) throw std::runtime_error("Unexpected end of data");
            std::memcpy(out, ptr, len);
            ptr += len;
            remaining -= len;
        }
        uint8_t ReadByte() { uint8_t v; Read(&v, 1); return v; }
    } reader{m_base, m_mapped.Size()};

    auto readInt = [&]() -> int {
        int32_t v;
        reader.Read(&v, sizeof(v));
        return static_cast<int>(v);
    };

    uint64_t magic;
    reader.Read(&magic, sizeof(magic));
    if (magic != kMagic)
        throw std::runtime_error("Snapshot magic mismatch");

    int32_t version;
    reader.Read(&version, sizeof(version));
    if (version != kVersion)
        throw std::runtime_error("Snapshot version mismatch");

    m_meta.RowCount = readInt();
    m_meta.UniqueCount = readInt();
    m_meta.NameBlobLength = readInt();
    m_meta.ChildrenLength = readInt();
    m_meta.AliasEntryCount = readInt();
    m_meta.AliasBlobLength = readInt();
    m_meta.OrphanCount = readInt();
    m_meta.TotalFiles = readInt();
    m_meta.TotalDirs = readInt();

    uint8_t sourceKind = reader.ReadByte();
    uint8_t idKind = reader.ReadByte();
    m_meta.Source = static_cast<SourceKind>(sourceKind);
    m_meta.IdKind = static_cast<IdKind>(idKind);

    reader.Read(&m_meta.VolumeSerialNumber, sizeof(m_meta.VolumeSerialNumber));

    reader.Read(&m_meta.RootId.low, sizeof(m_meta.RootId.low));
    reader.Read(&m_meta.RootId.high, sizeof(m_meta.RootId.high));
    reader.Read(&m_meta.JournalId, sizeof(m_meta.JournalId));
    reader.Read(&m_meta.NextUsn, sizeof(m_meta.NextUsn));

    auto read7Bit = [&]() -> int32_t {
        int32_t result = 0;
        int shift = 0;
        uint8_t b;
        do {
            b = reader.ReadByte();
            result |= static_cast<int32_t>(b & 0x7Fu) << shift;
            shift += 7;
        } while (b & 0x80);
        return result;
    };

    auto readString = [&]() -> std::string {
        int32_t len = read7Bit();
        if (len < 0) throw std::runtime_error("Negative string length");
        std::string s(static_cast<size_t>(len), '\0');
        reader.Read(s.data(), static_cast<size_t>(len));
        return s;
    };

    m_meta.SourceKey = readString();
    m_meta.SourceRoot = readString();
    m_meta.FileSystemType = readString();

    uint8_t complete = reader.ReadByte();
    m_meta.IsComplete = (complete != 0);

    m_meta.ExclusionRulesFingerprint = readString();
    m_meta.AliasProvidersFingerprint = readString();
    reader.Read(&m_meta.LastUpdated, sizeof(m_meta.LastUpdated));

    // Align to 16 bytes after header.
    size_t headerEnd = static_cast<size_t>(reader.remaining);
    size_t totalSize = m_mapped.Size();
    size_t headerSize = totalSize - headerEnd;
    size_t alignedHeaderSize = (headerSize + 15) & ~size_t{15};

    // Compute section offsets relative to m_base.
    const int row = m_meta.RowCount;
    const int uniq = m_meta.UniqueCount;

    int64_t sectionSizes[] = {
        4LL * row, 2LL * row, 4LL * row,
        8LL * uniq, 4LL * (uniq + 1LL),
        static_cast<int64_t>(m_meta.NameBlobLength),
        16LL * row, 8LL * row,
        4LL * row, 4LL * row, 4LL * row,
        4LL * (row + 1LL), 4LL * m_meta.ChildrenLength,
        4LL * (uniq + 1LL), 4LL * row,
        4LL * (uniq + 1LL), 4LL * (m_meta.AliasEntryCount + 1LL),
        static_cast<int64_t>(m_meta.AliasEntryCount),
        static_cast<int64_t>(m_meta.AliasBlobLength),
        4LL * m_meta.OrphanCount, 16LL * m_meta.OrphanCount,
        8LL * ((uniq + 63LL) / 64LL),
    };
    static_assert(static_cast<int>(SnapshotSection::Count) == std::size(sectionSizes),
                  "section count mismatch");

    int64_t acc = static_cast<int64_t>(alignedHeaderSize);
    for (int i = 0; i < static_cast<int>(SnapshotSection::Count); ++i) {
        int64_t rem = acc % 16;
        if (rem != 0) acc += (16 - rem);
        m_offsets[i] = acc;
        acc += sectionSizes[i];
    }
}

void Snapshot::Close() noexcept {
    m_mapped.Close();
    m_base = nullptr;
    m_meta = {};
    std::memset(m_offsets, 0, sizeof(m_offsets));
}

const std::byte* Snapshot::SectionPointer(int sectionIndex) const {
    return m_base + m_offsets[sectionIndex];
}

#define SPAN_CAST(T, section, count) \
    std::span<const T>(reinterpret_cast<const T*>(SectionPointer(static_cast<int>(section))), (count))

std::span<const uint32_t> Snapshot::NameIds() const { return SPAN_CAST(uint32_t, SnapshotSection::NameIds, RowCount()); }
std::span<const uint16_t> Snapshot::Flags() const { return SPAN_CAST(uint16_t, SnapshotSection::Flags, RowCount()); }
std::span<const int32_t> Snapshot::ParentIndexes() const { return SPAN_CAST(int32_t, SnapshotSection::ParentIndexes, RowCount()); }
std::span<const uint64_t> Snapshot::UniqueMasks() const { return SPAN_CAST(uint64_t, SnapshotSection::UniqueMasks, m_meta.UniqueCount); }
std::span<const uint32_t> Snapshot::NameOffsets() const { return SPAN_CAST(uint32_t, SnapshotSection::NameOffsets, m_meta.UniqueCount + 1); }
std::span<const uint8_t> Snapshot::NameBlob() const { return SPAN_CAST(uint8_t, SnapshotSection::NameBlob, m_meta.NameBlobLength); }
std::span<const UInt128> Snapshot::Ids() const { return SPAN_CAST(UInt128, SnapshotSection::Ids, RowCount()); }
std::span<const int64_t> Snapshot::Sizes() const { return SPAN_CAST(int64_t, SnapshotSection::Sizes, RowCount()); }
std::span<const uint32_t> Snapshot::CreationTimes() const { return SPAN_CAST(uint32_t, SnapshotSection::CreationTimes, RowCount()); }
std::span<const uint32_t> Snapshot::LastWriteTimes() const { return SPAN_CAST(uint32_t, SnapshotSection::LastWriteTimes, RowCount()); }
std::span<const uint32_t> Snapshot::LastAccessTimes() const { return SPAN_CAST(uint32_t, SnapshotSection::LastAccessTimes, RowCount()); }
std::span<const int32_t> Snapshot::ChildStarts() const { return SPAN_CAST(int32_t, SnapshotSection::ChildStarts, RowCount() + 1); }
std::span<const int32_t> Snapshot::Children() const { return SPAN_CAST(int32_t, SnapshotSection::Children, m_meta.ChildrenLength); }
std::span<const int32_t> Snapshot::UidStarts() const { return SPAN_CAST(int32_t, SnapshotSection::UidStarts, m_meta.UniqueCount + 1); }
std::span<const int32_t> Snapshot::UidRows() const { return SPAN_CAST(int32_t, SnapshotSection::UidRows, RowCount()); }
std::span<const int32_t> Snapshot::OrphanRows() const { return SPAN_CAST(int32_t, SnapshotSection::OrphanRows, m_meta.OrphanCount); }
std::span<const UInt128> Snapshot::OrphanFrns() const { return SPAN_CAST(UInt128, SnapshotSection::OrphanFrns, m_meta.OrphanCount); }
std::span<const uint64_t> Snapshot::UniqueAsciiBits() const {
    int words = (m_meta.UniqueCount + 63) / 64;
    return SPAN_CAST(uint64_t, SnapshotSection::UniqueAsciiBits, words);
}

#undef SPAN_CAST

std::span<const uint8_t> Snapshot::UniqueNameUtf8(int uid) const {
    const auto offsets = NameOffsets();
    const auto blob = NameBlob();
    uint32_t start = offsets[uid];
    uint32_t end = offsets[uid + 1];
    return blob.subspan(start, end - start);
}

std::string Snapshot::GetName(int row) const {
    const auto nameIds = NameIds();
    auto utf8 = UniqueNameUtf8(nameIds[row]);
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

bool Snapshot::IsDirectory(int row) const {
    const auto flags = Flags();
    return HasFlag(static_cast<FileRecordFlags>(flags[row]), FileRecordFlags::Directory);
}

bool Snapshot::IsDeleted(int row) const {
    const auto flags = Flags();
    return HasFlag(static_cast<FileRecordFlags>(flags[row]), FileRecordFlags::Deleted);
}

bool Snapshot::IsUniqueAscii(int uid) const {
    const auto bits = UniqueAsciiBits();
    return (bits[uid >> 6] >> (uid & 63)) & 1ULL;
}

std::span<const int32_t> Snapshot::ChildrenOf(int row) const {
    const auto starts = ChildStarts();
    const auto children = Children();
    int32_t start = starts[row];
    int32_t end = starts[row + 1];
    return children.subspan(start, end - start);
}

int Snapshot::FindRowById(UInt128 id) const {
    const auto ids = Ids();
    // Binary search (Ids are sorted).
    auto it = std::lower_bound(ids.begin(), ids.end(), id,
                               [](const UInt128& elem, const UInt128& val) {
                                   if (elem.low != val.low) return elem.low < val.low;
                                   return elem.high < val.high;
                               });
    if (it != ids.end() && it->low == id.low && it->high == id.high)
        return static_cast<int>(it - ids.begin());
    return -1;
}

bool Snapshot::TryGetOrphanParent(int row, UInt128& parentFrn) const {
    const auto orphanRows = OrphanRows();
    auto it = std::find(orphanRows.begin(), orphanRows.end(), row);
    if (it != orphanRows.end()) {
        auto idx = static_cast<int>(it - orphanRows.begin());
        const auto orphanFrns = OrphanFrns();
        parentFrn = orphanFrns[idx];
        return true;
    }
    return false;
}

} // namespace swiftlist::index_v2
