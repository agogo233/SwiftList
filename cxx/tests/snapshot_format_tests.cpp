#include "index_v2/snapshot_format.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace swiftlist::index_v2;

// --- Header round-trip (exercises 7-bit encoding, string, UInt128 serialization) ---

TEST(SnapshotFormatTest, HeaderRoundTrip) {
    Meta meta{};
    meta.RowCount = 100;
    meta.UniqueCount = 50;
    meta.NameBlobLength = 500;
    meta.ChildrenLength = 80;
    meta.AliasEntryCount = 10;
    meta.AliasBlobLength = 100;
    meta.OrphanCount = 5;
    meta.TotalFiles = 90;
    meta.TotalDirs = 10;
    meta.Source = SourceKind::LocalMft;
    meta.IdKind = IdKind::MftFrn;
    meta.VolumeSerialNumber = 0x12345678;
    meta.RootId = {0xAAAABBBBCCCCDDDDULL, 0x1111222233334444ULL};
    meta.JournalId = 0x5566778899AABBCCULL;
    meta.NextUsn = 0x1122334455667788LL;
    meta.SourceKey = "LocalMFT:C:";
    meta.SourceRoot = "C:\\";
    meta.FileSystemType = "NTFS";
    meta.IsComplete = true;
    meta.ExclusionRulesFingerprint = "abc123";
    meta.AliasProvidersFingerprint = "alias456";
    meta.LastUpdated = 1234567890LL;

    std::ostringstream s;
    WriteHeader(s, meta);
    std::string data = s.str();

    std::istringstream in(data);
    Meta readMeta = ReadHeader(in);

    EXPECT_EQ(readMeta.RowCount, 100);
    EXPECT_EQ(readMeta.UniqueCount, 50);
    EXPECT_EQ(readMeta.NameBlobLength, 500);
    EXPECT_EQ(readMeta.ChildrenLength, 80);
    EXPECT_EQ(readMeta.AliasEntryCount, 10);
    EXPECT_EQ(readMeta.AliasBlobLength, 100);
    EXPECT_EQ(readMeta.OrphanCount, 5);
    EXPECT_EQ(readMeta.TotalFiles, 90);
    EXPECT_EQ(readMeta.TotalDirs, 10);
    EXPECT_EQ(readMeta.Source, SourceKind::LocalMft);
    EXPECT_EQ(readMeta.IdKind, IdKind::MftFrn);
    EXPECT_EQ(readMeta.VolumeSerialNumber, 0x12345678);
    EXPECT_EQ(readMeta.RootId.low, 0xAAAABBBBCCCCDDDDULL);
    EXPECT_EQ(readMeta.RootId.high, 0x1111222233334444ULL);
    EXPECT_EQ(readMeta.JournalId, 0x5566778899AABBCCULL);
    EXPECT_EQ(readMeta.NextUsn, 0x1122334455667788LL);
    EXPECT_EQ(readMeta.SourceKey, "LocalMFT:C:");
    EXPECT_EQ(readMeta.SourceRoot, "C:\\");
    EXPECT_EQ(readMeta.FileSystemType, "NTFS");
    EXPECT_EQ(readMeta.IsComplete, true);
    EXPECT_EQ(readMeta.ExclusionRulesFingerprint, "abc123");
    EXPECT_EQ(readMeta.AliasProvidersFingerprint, "alias456");
    EXPECT_EQ(readMeta.LastUpdated, 1234567890LL);
}

TEST(SnapshotFormatTest, HeaderRoundTripEmptyStrings) {
    Meta meta{};
    meta.RowCount = 0;
    meta.UniqueCount = 0;
    meta.NameBlobLength = 0;
    meta.ChildrenLength = 0;
    meta.AliasEntryCount = 0;
    meta.AliasBlobLength = 0;
    meta.OrphanCount = 0;
    meta.SourceKey = "";
    meta.SourceRoot = "";
    meta.FileSystemType = "";
    meta.ExclusionRulesFingerprint = "";
    meta.AliasProvidersFingerprint = "";

    std::ostringstream s;
    WriteHeader(s, meta);

    std::istringstream in(s.str());
    Meta readMeta = ReadHeader(in);

    EXPECT_EQ(readMeta.RowCount, 0);
    EXPECT_EQ(readMeta.SourceKey, "");
    EXPECT_EQ(readMeta.IsComplete, false);
}

TEST(SnapshotFormatTest, HeaderLargeString) {
    // String longer than 127 bytes requires multi-byte 7-bit encoding.
    Meta meta{};
    meta.SourceKey = std::string(200, 'A');
    meta.SourceRoot = std::string(300, 'B');

    std::ostringstream s;
    WriteHeader(s, meta);

    std::istringstream in(s.str());
    Meta readMeta = ReadHeader(in);

    EXPECT_EQ(readMeta.SourceKey.size(), 200);
    EXPECT_EQ(readMeta.SourceRoot.size(), 300);
    EXPECT_EQ(readMeta.SourceKey, std::string(200, 'A'));
    EXPECT_EQ(readMeta.SourceRoot, std::string(300, 'B'));
}

TEST(SnapshotFormatTest, HeaderWrongMagicThrows) {
    std::string data(8, '\0'); // all zeros
    std::istringstream in(data);
    EXPECT_THROW(ReadHeader(in), std::runtime_error);
}

TEST(SnapshotFormatTest, HeaderWrongVersionThrows) {
    std::ostringstream s;
    uint64_t wrongMagic = kMagic;
    s.write(reinterpret_cast<const char*>(&wrongMagic), 8);
    int32_t wrongVersion = 999;
    s.write(reinterpret_cast<const char*>(&wrongVersion), 4);
    std::istringstream in(s.str());
    EXPECT_THROW(ReadHeader(in), std::runtime_error);
}

// --- Section offsets ---

TEST(SnapshotFormatTest, ComputeSectionOffsetsAlignment) {
    Meta meta{};
    meta.RowCount = 4;
    meta.UniqueCount = 2;
    meta.NameBlobLength = 10;
    meta.ChildrenLength = 3;
    meta.AliasEntryCount = 0;
    meta.AliasBlobLength = 0;
    meta.OrphanCount = 1;

    int64_t totalLength = 0;
    auto offsets = ComputeSectionOffsets(meta, &totalLength);

    // Each offset must be 16-byte aligned.
    for (auto off : offsets) {
        EXPECT_EQ(off % 16, 0);
    }
    // Total length must be 16-byte aligned.
    EXPECT_EQ(totalLength % 16, 0);
    // Offsets must be strictly increasing.
    for (size_t i = 1; i < offsets.size(); ++i) {
        EXPECT_GT(offsets[i], offsets[i - 1]);
    }
}

TEST(SnapshotFormatTest, ComputeSectionOffsetsCount) {
    Meta meta{};
    int64_t totalLength = 0;
    auto offsets = ComputeSectionOffsets(meta, &totalLength);
    EXPECT_EQ(offsets.size(), static_cast<int>(SnapshotSection::Count));
}

// --- FileRecordFlags bitwise ops ---

TEST(FileRecordFlagsTest, BitwiseOr) {
    auto combined = FileRecordFlags::Directory | FileRecordFlags::Hidden;
    EXPECT_TRUE(HasFlag(combined, FileRecordFlags::Directory));
    EXPECT_TRUE(HasFlag(combined, FileRecordFlags::Hidden));
}

TEST(FileRecordFlagsTest, BitwiseAnd) {
    auto combined = FileRecordFlags::Directory | FileRecordFlags::Deleted;
    EXPECT_TRUE(HasFlag(combined, FileRecordFlags::Directory));
    EXPECT_TRUE(HasFlag(combined, FileRecordFlags::Deleted));
    EXPECT_FALSE(HasFlag(combined, FileRecordFlags::Hidden));
}
