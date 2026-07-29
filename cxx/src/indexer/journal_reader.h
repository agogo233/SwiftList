#pragma once

#include "indexer/usn_record_parser.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace swiftlist::indexer {

// IOCTL codes.
inline constexpr uint32_t FSCTL_QUERY_USN_JOURNAL  = 0x000900f4;
inline constexpr uint32_t FSCTL_READ_USN_JOURNAL   = 0x000900bb;
inline constexpr uint32_t FSCTL_CREATE_USN_JOURNAL = 0x000900e7;
inline constexpr uint32_t FSCTL_ENUM_USN_DATA      = 0x000900b3;

// USN_JOURNAL_DATA_V0 structure (56 bytes).
struct UsnJournalData {
    uint64_t JournalId;
    int64_t  FirstUsn;
    int64_t  NextUsn;
    int64_t  LowestValidUsn;
    int64_t  MaxUsn;
    uint64_t MaximumSize;
    uint64_t AllocationDelta;
};

// READ_USN_JOURNAL_DATA_V0 structure (40 bytes).
struct ReadUsnJournalData {
    int64_t  StartUsn;
    uint32_t ReasonMask;
    uint32_t ReturnOnlyOnClose;
    uint64_t Timeout;
    uint64_t BytesToWaitFor;
    uint64_t UsnJournalID;
};

// NTFS volume data from FSCTL_GET_NTFS_VOLUME_DATA.
struct NtfsVolumeData {
    uint32_t BytesPerSector;
    uint32_t BytesPerCluster;
    uint32_t RecordSize;       // MFT record size
    uint64_t MftValidLen;      // valid MFT length in bytes
    int64_t  MftStartLcn;      // starting LCN of $MFT
};

// Queries the USN journal state.
bool QueryUsnJournal(HANDLE volume, UsnJournalData& data);

// Reads USN records from the journal starting at StartUsn.
// Invokes onRecord for each parsed record.
// Returns false on fatal error (e.g., journal deleted).
// Returns true if all records up to NextUsn have been read.
bool ReadUsnJournal(HANDLE volume, const UsnJournalData& journal,
                    int64_t startUsn, uint32_t reasonMask,
                    std::function<void(const UsnRecord&)> onRecord);

// Creates a new USN journal if one doesn't exist.
bool CreateUsnJournal(HANDLE volume);

// Gets NTFS volume data for MFT scanning.
bool GetNtfsVolumeData(HANDLE volume, NtfsVolumeData& data);

} // namespace swiftlist::indexer
