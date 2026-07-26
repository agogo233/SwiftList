#pragma once

#include "libengine/common/defs.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace swiftlist::indexer {

struct DataRun {
    int64_t Lcn;          // absolute logical cluster number
    uint64_t ClusterCount; // run length in clusters
};

struct MftNameEntry {
    UInt128 ParentFrn;
    std::string Name;      // UTF-8 filename
    uint32_t FileAttributes;
    uint64_t RealSize;
    uint32_t CreationUnix;
    uint32_t LastWriteUnix;
    uint32_t LastAccessUnix;
};

// Applies USA fixup to an NTFS FILE record buffer.
// Replaces the last 2 bytes of each sector with the corresponding USA entry.
// Returns false on malformed input.
bool ApplyFixup(std::span<uint8_t> buf, uint32_t bytesPerSector);

// Parses NTFS data runs from a raw attribute value buffer.
// Returns a list of (lcn, clusterCount) extents.
std::vector<DataRun> ParseDataRuns(std::span<const uint8_t> attributeValue);

// Parses names from a single MFT FILE record.
// The record must already have fixup applied.
// Returns all hard links (name entries) found in the record.
std::vector<MftNameEntry> CollectNames(std::span<const uint8_t> record,
                                       uint32_t bytesPerSector,
                                       int recordIndex);

// Reads a little-endian unsigned value of `n` bytes from buffer.
uint64_t ReadLE(const uint8_t* p, int n);

// Reads a little-endian signed value (sign-extended) of `n` bytes.
int64_t ReadSignedLE(const uint8_t* p, int n);

// Converts a Windows FILETIME (100-ns intervals since 1601-01-01) to Unix seconds.
// Returns 0 for invalid/negative values.
uint32_t FileTimeToUnixSeconds(int64_t fileTime);

} // namespace swiftlist::indexer
