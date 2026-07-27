#include "indexer/mft_index_scanner.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cwctype>

namespace swiftlist::indexer {

bool MftIndexScanner::Scan(wchar_t driveLetter, FileRecordStore& store, Callback cb) {
    VolumeHandle vol;
    if (!vol.Open(driveLetter)) return false;

    NtfsVolumeData vdata = {};
    if (!GetNtfsVolumeData(vol.Get(), vdata)) return false;

    if (vdata.RecordSize == 0 || vdata.MftValidLen == 0) return false;

    std::wstring mftPath = std::wstring(L"\\\\.\\") + static_cast<wchar_t>(towupper(driveLetter)) + L":\\$MFT";
    HANDLE hMft = CreateFileW(mftPath.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hMft == INVALID_HANDLE_VALUE) return false;

    uint64_t mftSize = vdata.MftValidLen;
    // Cap at 512MB: enough for ~500K file records. Larger MFTs are rare;
    // Phase B can switch to memory-mapped I/O if this becomes a limitation.
    if (mftSize > 512 * 1024 * 1024) mftSize = 512 * 1024 * 1024;

    std::vector<uint8_t> mftBuf(static_cast<size_t>(mftSize));
    DWORD bytesRead = 0;
    if (!ReadFile(hMft, mftBuf.data(), static_cast<DWORD>(mftSize), &bytesRead, nullptr)) {
        CloseHandle(hMft);
        return false;
    }
    CloseHandle(hMft);

    const uint32_t recordSize = vdata.RecordSize;
    const uint32_t bytesPerSector = vdata.BytesPerSector;
    const size_t numRecords = bytesRead / recordSize;

    for (size_t i = 0; i < numRecords; ++i) {
        size_t off = i * recordSize;
        if (off + recordSize > bytesRead) break;

        auto record = std::span<uint8_t>(mftBuf.data() + off, recordSize);
        if (record[0] != 'F' || record[1] != 'I' || record[2] != 'L' || record[3] != 'E') {
            continue;
        }

        std::vector<uint8_t> recCopy(record.begin(), record.end());
        if (!ApplyFixup(recCopy, bytesPerSector)) continue;

        auto names = CollectNames(recCopy, bytesPerSector, static_cast<int>(i));
        if (names.empty()) continue;

        UInt128 frn{ReadLE(recCopy.data() + 0x1C, 8), 0};

        for (const auto& name : names) {
            FileRecordInput input;
            input.Id = frn;
            input.ParentId = name.ParentFrn;
            input.Name = name.Name;
            input.Flags = FileRecordFlags::None;
            if (name.FileAttributes & kFileAttrDirectory) {
                input.Flags = FileRecordFlags::Directory;
            }
            input.Size = static_cast<int64_t>(name.RealSize);
            input.CreationTimeUnix = name.CreationUnix;
            input.LastWriteTimeUnix = name.LastWriteUnix;
            input.LastAccessTimeUnix = name.LastAccessUnix;

            store.Records.push_back(input);
            if (cb) cb(input);
        }
    }

    store.IsComplete = true;
    return true;
}

} // namespace swiftlist::indexer
