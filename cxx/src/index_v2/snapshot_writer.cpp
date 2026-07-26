#include "index_v2/snapshot_writer.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace swiftlist::index_v2 {

namespace {

void WriteRaw(std::ostream& s, const void* data, std::streamsize len) {
    s.write(static_cast<const char*>(data), len);
}

void PadToAlignment(std::ostream& s, int alignment) {
    auto pos = s.tellp();
    auto rem = static_cast<int>(pos % alignment);
    if (rem != 0) {
        int pad = alignment - rem;
        std::vector<char> zeros(static_cast<size_t>(pad), 0);
        s.write(zeros.data(), pad);
    }
}

// Compute section offsets starting from 'headerEnd' (already aligned to 16).
std::vector<int64_t> ComputeSectionOffsetsFrom(const Meta& meta, int64_t headerEnd,
                                                int64_t* totalLength) {
    const int N = static_cast<int>(SnapshotSection::Count);
    const int row = meta.RowCount;
    const int uniq = meta.UniqueCount;

    int64_t sizes[] = {
        4LL * row, 2LL * row, 4LL * row,
        8LL * uniq, 4LL * (uniq + 1LL),
        static_cast<int64_t>(meta.NameBlobLength),
        16LL * row, 8LL * row,
        4LL * row, 4LL * row, 4LL * row,
        4LL * (row + 1LL), 4LL * meta.ChildrenLength,
        4LL * (uniq + 1LL), 4LL * row,
        4LL * (uniq + 1LL), 4LL * (meta.AliasEntryCount + 1LL),
        static_cast<int64_t>(meta.AliasEntryCount),
        static_cast<int64_t>(meta.AliasBlobLength),
        4LL * meta.OrphanCount, 16LL * meta.OrphanCount,
        8LL * ((uniq + 63LL) / 64LL),
    };
    static_assert(N == std::size(sizes), "section count mismatch");

    std::vector<int64_t> offsets(N);
    int64_t acc = headerEnd;
    for (int i = 0; i < N; ++i) {
        int64_t rem = acc % 16;
        if (rem != 0) acc += (16 - rem);
        offsets[i] = acc;
        acc += sizes[i];
    }
    // Align end.
    int64_t rem = acc % 16;
    if (rem != 0) acc += (16 - rem);
    if (totalLength) *totalLength = acc;
    return offsets;
}

void WriteSection(std::ostream& s, const void* data, int64_t offset, std::streamsize len) {
    auto current = s.tellp();
    if (current > offset) throw std::runtime_error("Stream position past section offset");
    // Pad to section start.
    if (current < offset) {
        std::vector<char> zeros(static_cast<size_t>(offset - current), 0);
        s.write(zeros.data(), static_cast<std::streamsize>(offset - current));
    }
    if (len > 0) {
        s.write(static_cast<const char*>(data), len);
    }
}

} // namespace

void SnapshotWriter::WriteToStream(const SnapshotColumns& columns, std::ostream& s) {
    const auto& meta = columns.meta;
    const int row = meta.RowCount;
    const int uniq = meta.UniqueCount;

    // Write header.
    WriteHeader(s, meta);
    PadToAlignment(s, 16);

    auto headerEnd = s.tellp();

    // Compute section offsets.
    int64_t totalLength = 0;
    auto offsets = ComputeSectionOffsetsFrom(meta, headerEnd, &totalLength);

    // Write sections in order.
    // NameIds
    WriteSection(s, columns.nameIds.data(), offsets[0],
                 static_cast<std::streamsize>(sizeof(uint32_t) * row));
    // Flags
    WriteSection(s, columns.flags.data(), offsets[1],
                 static_cast<std::streamsize>(sizeof(uint16_t) * row));
    // ParentIndexes
    WriteSection(s, columns.parentIndexes.data(), offsets[2],
                 static_cast<std::streamsize>(sizeof(int32_t) * row));
    // UniqueMasks
    WriteSection(s, columns.uniqueMasks.data(), offsets[3],
                 static_cast<std::streamsize>(sizeof(uint64_t) * uniq));
    // NameOffsets
    WriteSection(s, columns.nameOffsets.data(), offsets[4],
                 static_cast<std::streamsize>(sizeof(uint32_t) * (uniq + 1)));
    // NameBlob
    WriteSection(s, columns.nameBlob.data(), offsets[5],
                 static_cast<std::streamsize>(columns.nameBlob.size()));
    // Ids (UInt128)
    WriteSection(s, columns.ids.data(), offsets[6],
                 static_cast<std::streamsize>(sizeof(UInt128) * row));
    // Sizes
    WriteSection(s, columns.sizes.data(), offsets[7],
                 static_cast<std::streamsize>(sizeof(int64_t) * row));
    // CreationTimes
    WriteSection(s, columns.creationTimes.data(), offsets[8],
                 static_cast<std::streamsize>(sizeof(uint32_t) * row));
    // LastWriteTimes
    WriteSection(s, columns.lastWriteTimes.data(), offsets[9],
                 static_cast<std::streamsize>(sizeof(uint32_t) * row));
    // LastAccessTimes
    WriteSection(s, columns.lastAccessTimes.data(), offsets[10],
                 static_cast<std::streamsize>(sizeof(uint32_t) * row));
    // ChildStarts
    WriteSection(s, columns.childStarts.data(), offsets[11],
                 static_cast<std::streamsize>(sizeof(int32_t) * (row + 1)));
    // Children
    WriteSection(s, columns.children.data(), offsets[12],
                 static_cast<std::streamsize>(sizeof(int32_t) * meta.ChildrenLength));
    // UidStarts
    WriteSection(s, columns.uidStarts.data(), offsets[13],
                 static_cast<std::streamsize>(sizeof(int32_t) * (uniq + 1)));
    // UidRows
    WriteSection(s, columns.uidRows.data(), offsets[14],
                 static_cast<std::streamsize>(sizeof(int32_t) * row));
    // AliasStarts (empty for now)
    if (meta.UniqueCount > 0) {
        std::vector<int32_t> aliasStarts(static_cast<size_t>(meta.UniqueCount + 1), 0);
        WriteSection(s, aliasStarts.data(), offsets[15],
                     static_cast<std::streamsize>(sizeof(int32_t) * (uniq + 1)));
    } else {
        int32_t zero = 0;
        WriteSection(s, &zero, offsets[15], sizeof(int32_t));
    }
    // AliasEntryOffsets (empty)
    if (meta.AliasEntryCount > 0) {
        std::vector<uint32_t> aeo(static_cast<size_t>(meta.AliasEntryCount + 1), 0);
        WriteSection(s, aeo.data(), offsets[16],
                     static_cast<std::streamsize>(sizeof(uint32_t) * (meta.AliasEntryCount + 1)));
    } else {
        uint32_t zero = 0;
        WriteSection(s, &zero, offsets[16], sizeof(uint32_t));
    }
    // AliasProviderIds (empty)
    // AliasBlob (empty)
    // OrphanRows
    WriteSection(s, columns.orphanRows.data(), offsets[19],
                 static_cast<std::streamsize>(sizeof(int32_t) * meta.OrphanCount));
    // OrphanFrns
    WriteSection(s, columns.orphanFrns.data(), offsets[20],
                 static_cast<std::streamsize>(sizeof(UInt128) * meta.OrphanCount));
    // UniqueAsciiBits
    int asciiWords = (uniq + 63) / 64;
    WriteSection(s, columns.uniqueAsciiBits.data(), offsets[21],
                 static_cast<std::streamsize>(sizeof(uint64_t) * asciiWords));
}

void SnapshotWriter::Write(const std::vector<FileRecordInput>& records,
                            const Meta& metaTemplate,
                            const std::string& path) {
    auto columns = SnapshotBuilder::Build(records, metaTemplate);

    // Write to temp file.
    std::string tempPath = path + ".tmp";
    {
        std::ofstream ofs(tempPath, std::ios::binary);
        if (!ofs) throw std::runtime_error("Cannot open temp file for writing: " + tempPath);
        WriteToStream(columns, ofs);
        ofs.flush();
        if (!ofs) throw std::runtime_error("Write to snapshot failed");
    }

    // Atomic rename.
#ifdef _WIN32
    // On Windows, use MoveFileEx with MOVEFILE_REPLACE_EXISTING.
    std::wstring wideTemp(tempPath.begin(), tempPath.end());
    std::wstring wideFinal(path.begin(), path.end());
    BOOL ok = ::MoveFileExW(wideTemp.c_str(), wideFinal.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        // Fallback: delete then rename.
        ::DeleteFileW(wideFinal.c_str());
        ok = ::MoveFileExW(wideTemp.c_str(), wideFinal.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!ok) throw std::runtime_error("Atomic rename failed");
    }
#else
    // POSIX rename() is atomic and overwrites.
    int rc = std::rename(tempPath.c_str(), path.c_str());
    if (rc != 0) throw std::runtime_error("Atomic rename failed");
#endif
}

} // namespace swiftlist::index_v2
