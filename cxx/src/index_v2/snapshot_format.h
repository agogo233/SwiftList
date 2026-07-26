#pragma once

#include "common/defs.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace swiftlist::index_v2 {

// Binary format constants. Must stay in sync with C# SnapshotFormat.
inline constexpr uint64_t kMagic = 0x0000005844494C53ULL; // "SLIDX\0\0\0" little-endian
inline constexpr int kVersion = 4;
inline constexpr int kSectionAlignment = 16;

enum class SnapshotSection : int {
    NameIds = 0,            // uint32[RowCount]
    Flags,                  // uint16[RowCount]
    ParentIndexes,          // int32[RowCount]
    UniqueMasks,            // uint64[UniqueCount]
    NameOffsets,            // uint32[UniqueCount + 1]
    NameBlob,               // byte[NameBlobLength]
    Ids,                    // UInt128[RowCount]
    Sizes,                  // int64[RowCount]
    CreationTimes,          // uint32[RowCount]
    LastWriteTimes,         // uint32[RowCount]
    LastAccessTimes,        // uint32[RowCount]
    ChildStarts,            // int32[RowCount + 1]
    Children,               // int32[ChildrenLength]
    UidStarts,              // int32[UniqueCount + 1]
    UidRows,                // int32[RowCount]
    AliasStarts,            // int32[UniqueCount + 1]
    AliasEntryOffsets,      // uint32[AliasEntryCount + 1]
    AliasProviderIds,       // byte[AliasEntryCount]
    AliasBlob,              // byte[AliasBlobLength]
    OrphanRows,             // int32[OrphanCount]
    OrphanFrns,             // UInt128[OrphanCount]
    UniqueAsciiBits,        // uint64[(UniqueCount + 63) / 64]
    Count                   // sentinel: total number of sections
};

enum class SourceKind : uint8_t {
    LocalMft = 1,
    NetworkMappedDrive = 2,
};

enum class IdKind : uint8_t {
    MftFrn = 1,
    SourceLocalId64 = 2,
};

// FileRecordFlags mirrors C# FileRecordFlags (ushort enum).
enum class FileRecordFlags : uint16_t {
    None       = 0,
    Directory  = 1,
    Deleted    = 2,
    SourceRoot = 4,
    Hidden     = 8,
    System     = 16,
    ReadOnly   = 32,
    Compressed = 64,
    Encrypted  = 128,
    Listed     = 256,
};

constexpr FileRecordFlags operator|(FileRecordFlags a, FileRecordFlags b) {
    return static_cast<FileRecordFlags>(
        static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

constexpr FileRecordFlags operator&(FileRecordFlags a, FileRecordFlags b) {
    return static_cast<FileRecordFlags>(
        static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

[[nodiscard]] constexpr bool HasFlag(FileRecordFlags value, FileRecordFlags flag) {
    return (static_cast<uint16_t>(value) & static_cast<uint16_t>(flag)) != 0;
}

// In-memory representation of the snapshot header.
struct Meta {
    int RowCount = 0;
    int UniqueCount = 0;
    int NameBlobLength = 0;
    int ChildrenLength = 0;
    int AliasEntryCount = 0;
    int AliasBlobLength = 0;
    int OrphanCount = 0;
    int TotalFiles = 0;
    int TotalDirs = 0;
    SourceKind Source = SourceKind::LocalMft;
    IdKind IdKind = IdKind::MftFrn;
    uint32_t VolumeSerialNumber = 0;
    UInt128 RootId{};
    uint64_t JournalId = 0;
    int64_t NextUsn = 0;
    std::string SourceKey;
    std::string SourceRoot;
    std::string FileSystemType;
    bool IsComplete = false;
    std::string ExclusionRulesFingerprint;
    std::string AliasProvidersFingerprint;
    int64_t LastUpdated = 0; // UTC ticks
};

// Read header from a binary stream. Throws std::runtime_error on mismatch.
Meta ReadHeader(std::istream& stream);

// Write header to a binary stream.
void WriteHeader(std::ostream& stream, const Meta& meta);

// Compute each section's file offset given a filled Meta.
// Returns the section offsets and total file length via output params.
std::vector<int64_t> ComputeSectionOffsets(const Meta& meta, int64_t* totalLength);

} // namespace swiftlist::index_v2
