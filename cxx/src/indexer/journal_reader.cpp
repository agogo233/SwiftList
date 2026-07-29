#include "indexer/journal_reader.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace swiftlist::indexer {

bool QueryUsnJournal(HANDLE volume, UsnJournalData& data) {
#ifdef _WIN32
    DWORD bytesReturned = 0;
    BOOL ok = ::DeviceIoControl(
        volume,
        FSCTL_QUERY_USN_JOURNAL,
        nullptr, 0,
        &data, sizeof(data),
        &bytesReturned,
        nullptr);
    return ok && bytesReturned >= sizeof(data);
#else
    (void)volume;
    (void)data;
    return false;
#endif
}

bool ReadUsnJournal(HANDLE volume, const UsnJournalData& journal,
                    int64_t startUsn, uint32_t reasonMask,
                    std::function<void(const UsnRecord&)> onRecord) {
#ifdef _WIN32
    if (startUsn >= journal.NextUsn) return true; // nothing to read

    ReadUsnJournalData input{};
    input.StartUsn = startUsn;
    input.ReasonMask = reasonMask;
    input.ReturnOnlyOnClose = 0;
    input.Timeout = 0;
    input.BytesToWaitFor = 0;
    input.UsnJournalID = journal.JournalId;

    std::vector<uint8_t> buf(1 << 20); // 1MB buffer
    DWORD bytesReturned = 0;

    BOOL ok = ::DeviceIoControl(
        volume,
        FSCTL_READ_USN_JOURNAL,
        &input, sizeof(input),
        buf.data(), static_cast<DWORD>(buf.size()),
        &bytesReturned,
        nullptr);

    if (!ok) return false;

    if (bytesReturned < 8) return true; // no records

    // First 8 bytes = next USN.
    int64_t nextUsn;
    std::memcpy(&nextUsn, buf.data(), 8);

    // Parse records starting at offset 8.
    size_t offset = 8;
    while (offset + 8 <= bytesReturned) {
        uint32_t recordLen;
        std::memcpy(&recordLen, buf.data() + offset, 4);
        if (recordLen == 0) break;
        if (offset + recordLen > bytesReturned) break;

        UsnRecord record;
        if (ParseUsnRecord(std::span<const uint8_t>(buf.data() + offset, recordLen), record)) {
            onRecord(record);
        }

        offset += recordLen;
    }

    return true;
#else
    (void)volume;
    (void)journal;
    (void)startUsn;
    (void)reasonMask;
    (void)onRecord;
    return false;
#endif
}

bool CreateUsnJournal(HANDLE volume) {
#ifdef _WIN32
    // CREATE_USN_JOURNAL_DATA with default parameters.
    struct CreateUsnJournalData {
        uint64_t MaximumSize;
        uint64_t AllocationDelta;
    };
    CreateUsnJournalData createData{0, 0};

    DWORD bytesReturned = 0;
    BOOL ok = ::DeviceIoControl(
        volume,
        FSCTL_CREATE_USN_JOURNAL,
        &createData, sizeof(createData),
        nullptr, 0,
        &bytesReturned,
        nullptr);
    return ok;
#else
    (void)volume;
    return false;
#endif
}

bool GetNtfsVolumeData(HANDLE volume, NtfsVolumeData& data) {
#ifdef _WIN32
    // FSCTL_GET_NTFS_VOLUME_DATA returns a variable-length structure.
    // We read the fixed fields we need.
    uint8_t buf[128]{};
    DWORD bytesReturned = 0;
    BOOL ok = ::DeviceIoControl(
        volume,
        0x00090064, // FSCTL_GET_NTFS_VOLUME_DATA
        nullptr, 0,
        buf, sizeof(buf),
        &bytesReturned,
        nullptr);

    if (!ok || bytesReturned < 72) return false;

    // Offsets from C# code:
    // +40: BytesPerSector (4)
    // +44: BytesPerCluster (4)
    // +48: RecordSize (4)
    // +56: MftValidLen (8)
    // +64: MftStartLcn (8)
    std::memcpy(&data.BytesPerSector, buf + 40, 4);
    std::memcpy(&data.BytesPerCluster, buf + 44, 4);
    std::memcpy(&data.RecordSize, buf + 48, 4);
    std::memcpy(&data.MftValidLen, buf + 56, 8);
    std::memcpy(&data.MftStartLcn, buf + 64, 8);
    return true;
#else
    (void)volume;
    (void)data;
    return false;
#endif
}

} // namespace swiftlist::indexer
