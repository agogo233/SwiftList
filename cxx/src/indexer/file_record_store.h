#pragma once

#include "index_v2/snapshot_builder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swiftlist::indexer {

enum class SourceKind : uint8_t {
    LocalMft = 1,
    NetworkMappedDrive = 2,
};

enum class IdKind : uint8_t {
    MftFrn = 1,
    SourceLocalId64 = 2,
};

// In-memory store of indexed file records before snapshot serialization.
struct FileRecordStore {
    std::string SourceKey;
    SourceKind Source = SourceKind::LocalMft;
    IdKind Id = IdKind::MftFrn;
    std::string FileSystemType;
    uint32_t VolumeSerialNumber = 0;
    UInt128 RootId{};
    uint64_t JournalId = 0;
    int64_t NextUsn = 0;
    bool IsComplete = false;
    std::string ExclusionRulesFingerprint;

    std::vector<FileRecordInput> Records;
};

} // namespace swiftlist::indexer
