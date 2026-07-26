#pragma once

#include "index_v2/snapshot_format.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace swiftlist::index_v2 {

// A single file/directory record as input to snapshot construction.
struct FileRecordInput {
    UInt128 Id;
    UInt128 ParentId;
    std::string Name; // UTF-8 filename (not full path)
    FileRecordFlags Flags = FileRecordFlags::None;
    int64_t Size = 0;
    uint32_t CreationTimeUnix = 0;
    uint32_t LastWriteTimeUnix = 0;
    uint32_t LastAccessTimeUnix = 0;
};

// All columnar data for a snapshot, sorted by Id.
// SnapshotWriter serializes this to disk.
struct SnapshotColumns {
    Meta meta;
    std::vector<uint8_t> nameBlob;
    std::vector<uint32_t> nameIds;       // [RowCount]
    std::vector<uint16_t> flags;          // [RowCount]
    std::vector<int32_t> parentIndexes;   // [RowCount] (-1 = unresolved)
    std::vector<uint64_t> uniqueMasks;    // [UniqueCount]
    std::vector<uint32_t> nameOffsets;    // [UniqueCount + 1]
    std::vector<UInt128> ids;             // [RowCount]
    std::vector<int64_t> sizes;           // [RowCount]
    std::vector<uint32_t> creationTimes;  // [RowCount]
    std::vector<uint32_t> lastWriteTimes; // [RowCount]
    std::vector<uint32_t> lastAccessTimes;// [RowCount]
    std::vector<int32_t> childStarts;     // [RowCount + 1] CSR format
    std::vector<int32_t> children;        // [ChildrenLength]
    std::vector<int32_t> uidStarts;       // [UniqueCount + 1]
    std::vector<int32_t> uidRows;         // [RowCount]
    std::vector<int32_t> orphanRows;      // [OrphanCount]
    std::vector<UInt128> orphanFrns;      // [OrphanCount]
    std::vector<uint64_t> uniqueAsciiBits;// [(UniqueCount+63)/64]
};

// Builds columnar snapshot data from FileRecordInput entries.
// Split out from SnapshotWriter purely to keep each file under the
// repo's per-file line limit; this class has no state of its own,
// it always operates on the record vector the caller owns.
class SnapshotBuilder {
public:
    // Constructs columns from records. Records are sorted by Id internally.
    static SnapshotColumns Build(std::vector<FileRecordInput> records, const Meta& metaTemplate);
};

} // namespace swiftlist::index_v2
