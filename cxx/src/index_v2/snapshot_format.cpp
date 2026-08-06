#include "index_v2/snapshot_format.h"

#include <algorithm>
#include <cassert>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace swiftlist::index_v2 {

namespace {

// Maximum string length read from snapshot to prevent large allocation on
// corrupt/malicious input (64 MiB).
inline constexpr int32_t kMaxStringLength = 64 * 1024 * 1024;

void Write7BitEncoded(std::ostream& s, int32_t value) {
    uint32_t v = static_cast<uint32_t>(value);
    while (v >= 0x80) {
        s.put(static_cast<char>((v & 0x7Fu) | 0x80u));
        v >>= 7;
    }
    s.put(static_cast<char>(v));
}

int32_t Read7BitEncoded(std::istream& s) {
    int32_t result = 0;
    int shift = 0;
    char b;
    do {
        b = static_cast<char>(s.get());
        if (!s) throw std::runtime_error("Unexpected EOF reading 7-bit encoded int");
        result |= static_cast<int32_t>(static_cast<uint8_t>(b) & 0x7Fu) << shift;
        shift += 7;
    } while (static_cast<uint8_t>(b) & 0x80);
    return result;
}

void WriteString(std::ostream& s, const std::string& str) {
    Write7BitEncoded(s, static_cast<int32_t>(str.size()));
    s.write(str.data(), static_cast<std::streamsize>(str.size()));
}

std::string ReadString(std::istream& s) {
    auto len = Read7BitEncoded(s);
    if (len < 0) throw std::runtime_error("Negative string length in snapshot");
    if (len > kMaxStringLength)
        throw std::runtime_error("String length exceeds maximum in snapshot");
    std::string result(static_cast<size_t>(len), '\0');
    s.read(result.data(), len);
    if (!s) throw std::runtime_error("Unexpected EOF reading string");
    return result;
}

void WriteUInt128(std::ostream& s, const UInt128& v) {
    // Little-endian: low first, then high (matches C# BinaryWriter).
    s.write(reinterpret_cast<const char*>(&v.low), sizeof(v.low));
    s.write(reinterpret_cast<const char*>(&v.high), sizeof(v.high));
}

UInt128 ReadUInt128(std::istream& s) {
    UInt128 v;
    s.read(reinterpret_cast<char*>(&v.low), sizeof(v.low));
    s.read(reinterpret_cast<char*>(&v.high), sizeof(v.high));
    if (!s) throw std::runtime_error("Unexpected EOF reading UInt128");
    return v;
}

void WriteBytes(std::ostream& s, const void* data, std::streamsize len) {
    s.write(static_cast<const char*>(data), len);
}

void AlignStream(std::ostream& s, int alignment) {
    auto pos = s.tellp();
    auto rem = static_cast<int>(pos % alignment);
    if (rem != 0) {
        int pad = alignment - rem;
        for (int i = 0; i < pad; ++i) s.put('\0');
    }
}

} // namespace

Meta ReadHeader(std::istream& stream) {
    Meta meta;

    uint64_t magic;
    stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != kMagic)
        throw std::runtime_error("Snapshot magic mismatch");

    int32_t version;
    stream.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != kVersion)
        throw std::runtime_error("Snapshot version mismatch");

    auto readInt = [&]() -> int {
        int32_t v;
        stream.read(reinterpret_cast<char*>(&v), sizeof(v));
        if (!stream) throw std::runtime_error("Unexpected EOF reading int32");
        return static_cast<int>(v);
    };

    meta.RowCount = readInt();
    meta.UniqueCount = readInt();
    meta.NameBlobLength = readInt();
    meta.ChildrenLength = readInt();
    meta.AliasEntryCount = readInt();
    meta.AliasBlobLength = readInt();
    meta.OrphanCount = readInt();
    meta.TotalFiles = readInt();
    meta.TotalDirs = readInt();

    uint8_t sourceKind = 0, idKind = 0;
    stream.read(reinterpret_cast<char*>(&sourceKind), 1);
    stream.read(reinterpret_cast<char*>(&idKind), 1);
    meta.Source = static_cast<SourceKind>(sourceKind);
    meta.IdKind = static_cast<IdKind>(idKind);

    stream.read(reinterpret_cast<char*>(&meta.VolumeSerialNumber),
               sizeof(meta.VolumeSerialNumber));
    meta.RootId = ReadUInt128(stream);
    stream.read(reinterpret_cast<char*>(&meta.JournalId), sizeof(meta.JournalId));
    stream.read(reinterpret_cast<char*>(&meta.NextUsn), sizeof(meta.NextUsn));

    meta.SourceKey = ReadString(stream);
    meta.SourceRoot = ReadString(stream);
    meta.FileSystemType = ReadString(stream);

    uint8_t complete = 0;
    stream.read(reinterpret_cast<char*>(&complete), 1);
    meta.IsComplete = (complete != 0);

    meta.ExclusionRulesFingerprint = ReadString(stream);
    meta.AliasProvidersFingerprint = ReadString(stream);
    stream.read(reinterpret_cast<char*>(&meta.LastUpdated), sizeof(meta.LastUpdated));

    return meta;
}

void WriteHeader(std::ostream& stream, const Meta& meta) {
    WriteBytes(stream, &kMagic, sizeof(kMagic));

    int32_t version = kVersion;
    WriteBytes(stream, &version, sizeof(version));

    auto writeInt = [&](int v) {
        int32_t sv = static_cast<int32_t>(v);
        WriteBytes(stream, &sv, sizeof(sv));
    };

    writeInt(meta.RowCount);
    writeInt(meta.UniqueCount);
    writeInt(meta.NameBlobLength);
    writeInt(meta.ChildrenLength);
    writeInt(meta.AliasEntryCount);
    writeInt(meta.AliasBlobLength);
    writeInt(meta.OrphanCount);
    writeInt(meta.TotalFiles);
    writeInt(meta.TotalDirs);

    uint8_t sourceKind = static_cast<uint8_t>(meta.Source);
    uint8_t idKind = static_cast<uint8_t>(meta.IdKind);
    stream.write(reinterpret_cast<const char*>(&sourceKind), 1);
    stream.write(reinterpret_cast<const char*>(&idKind), 1);

    WriteBytes(stream, &meta.VolumeSerialNumber, sizeof(meta.VolumeSerialNumber));
    WriteUInt128(stream, meta.RootId);
    WriteBytes(stream, &meta.JournalId, sizeof(meta.JournalId));
    WriteBytes(stream, &meta.NextUsn, sizeof(meta.NextUsn));

    WriteString(stream, meta.SourceKey);
    WriteString(stream, meta.SourceRoot);
    WriteString(stream, meta.FileSystemType);

    uint8_t complete = meta.IsComplete ? 1 : 0;
    stream.write(reinterpret_cast<const char*>(&complete), 1);

    WriteString(stream, meta.ExclusionRulesFingerprint);
    WriteString(stream, meta.AliasProvidersFingerprint);
    WriteBytes(stream, &meta.LastUpdated, sizeof(meta.LastUpdated));
}

std::vector<int64_t> ComputeSectionOffsets(const Meta& meta, int64_t* totalLength) {
    const int N = static_cast<int>(SnapshotSection::Count);
    const int row = meta.RowCount;
    const int uniq = meta.UniqueCount;

    // Byte size of each section (before alignment padding).
    int64_t sizes[] = {
        4LL * row,                              // NameIds
        2LL * row,                              // Flags
        4LL * row,                              // ParentIndexes
        8LL * uniq,                             // UniqueMasks
        4LL * (uniq + 1LL),                     // NameOffsets
        static_cast<int64_t>(meta.NameBlobLength), // NameBlob
        16LL * row,                             // Ids
        8LL * row,                              // Sizes
        4LL * row,                              // CreationTimes
        4LL * row,                              // LastWriteTimes
        4LL * row,                              // LastAccessTimes
        4LL * (row + 1LL),                      // ChildStarts
        4LL * meta.ChildrenLength,              // Children
        4LL * (uniq + 1LL),                     // UidStarts
        4LL * row,                              // UidRows
        4LL * (uniq + 1LL),                     // AliasStarts
        4LL * (meta.AliasEntryCount + 1LL),     // AliasEntryOffsets
        static_cast<int64_t>(meta.AliasEntryCount), // AliasProviderIds
        static_cast<int64_t>(meta.AliasBlobLength), // AliasBlob
        4LL * meta.OrphanCount,                 // OrphanRows
        16LL * meta.OrphanCount,                // OrphanFrns
        8LL * ((uniq + 63LL) / 64LL),           // UniqueAsciiBits
    };
    static_assert(N == std::size(sizes), "section count mismatch");

    // Align header end to 16 bytes (mirrors C# behavior).
    // We don't know the header size exactly here; caller handles pre-section alignment.
    std::vector<int64_t> offsets(N);

    if (totalLength) {
        int64_t acc = 0;
        for (int i = 0; i < N; ++i) {
            // Align start of each section.
            int64_t rem = acc % kSectionAlignment;
            if (rem != 0) acc += (kSectionAlignment - rem);
            offsets[i] = acc;
            acc += sizes[i];
        }
        // Align end to 16 bytes.
        int64_t rem = acc % kSectionAlignment;
        if (rem != 0) acc += (kSectionAlignment - rem);
        *totalLength = acc;
    }
    return offsets;
}

} // namespace swiftlist::index_v2
