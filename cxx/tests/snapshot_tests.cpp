#include "index_v2/snapshot_builder.h"
#include "index_v2/snapshot_writer.h"
#include "index_v2/snapshot.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace swiftlist::index_v2;

namespace {

Meta MakeTestMeta() {
    Meta meta{};
    meta.Source = SourceKind::LocalMft;
    meta.IdKind = IdKind::MftFrn;
    meta.VolumeSerialNumber = 0x12345678;
    meta.RootId = {0, 1};
    meta.JournalId = 42;
    meta.NextUsn = 1000;
    meta.SourceKey = "TestDrive";
    meta.SourceRoot = "C:\\";
    meta.FileSystemType = "NTFS";
    meta.IsComplete = true;
    meta.ExclusionRulesFingerprint = "";
    meta.AliasProvidersFingerprint = "";
    meta.LastUpdated = 0;
    return meta;
}

// Helper to create a simple file record.
FileRecordInput MakeRecord(uint64_t id, uint64_t parentId, std::string name,
                           FileRecordFlags flags = FileRecordFlags::None,
                           int64_t size = 0) {
    FileRecordInput r{};
    r.Id = {id, 0};
    r.ParentId = {parentId, 0};
    r.Name = std::move(name);
    r.Flags = flags;
    r.Size = size;
    r.CreationTimeUnix = 1000 + static_cast<uint32_t>(id);
    r.LastWriteTimeUnix = 2000 + static_cast<uint32_t>(id);
    r.LastAccessTimeUnix = 3000 + static_cast<uint32_t>(id);
    return r;
}

} // namespace

// --- SnapshotBuilder ---

TEST(SnapshotBuilderTest, EmptyRecords) {
    auto columns = SnapshotBuilder::Build({}, MakeTestMeta());
    EXPECT_EQ(columns.meta.RowCount, 0);
    EXPECT_EQ(columns.meta.UniqueCount, 0);
    EXPECT_EQ(columns.ids.size(), 0);
}

TEST(SnapshotBuilderTest, SingleRecord) {
    std::vector<FileRecordInput> records = {
        MakeRecord(1, 0, "root.txt", FileRecordFlags::Directory, 0),
    };
    auto columns = SnapshotBuilder::Build(records, MakeTestMeta());

    EXPECT_EQ(columns.meta.RowCount, 1);
    EXPECT_EQ(columns.meta.UniqueCount, 1);
    EXPECT_EQ(columns.ids.size(), 1);
    EXPECT_EQ(columns.ids[0].low, 1);
    EXPECT_EQ(columns.parentIndexes[0], -1); // parent 0 not found in records
    EXPECT_EQ(columns.flags[0], static_cast<uint16_t>(FileRecordFlags::Directory));
}

TEST(SnapshotBuilderTest, SortedById) {
    std::vector<FileRecordInput> records = {
        MakeRecord(3, 0, "c.txt"),
        MakeRecord(1, 0, "a.txt"),
        MakeRecord(2, 0, "b.txt"),
    };
    auto columns = SnapshotBuilder::Build(records, MakeTestMeta());

    EXPECT_EQ(columns.ids[0].low, 1);
    EXPECT_EQ(columns.ids[1].low, 2);
    EXPECT_EQ(columns.ids[2].low, 3);
}

TEST(SnapshotBuilderTest, ParentResolution) {
    // Parent is present in the record set.
    std::vector<FileRecordInput> records = {
        MakeRecord(1, 0, "folder", FileRecordFlags::Directory),
        MakeRecord(2, 1, "file.txt"),
    };
    auto columns = SnapshotBuilder::Build(records, MakeTestMeta());

    EXPECT_EQ(columns.parentIndexes[0], -1); // parent 0 not in set
    EXPECT_EQ(columns.parentIndexes[1], 0);  // parent is row 0
}

TEST(SnapshotBuilderTest, NameDeduplication) {
    // Two records with the same name should share a UID.
    std::vector<FileRecordInput> records = {
        MakeRecord(1, 0, "readme.txt"),
        MakeRecord(2, 0, "readme.txt"),
        MakeRecord(3, 0, "other.txt"),
    };
    auto columns = SnapshotBuilder::Build(records, MakeTestMeta());

    EXPECT_EQ(columns.meta.UniqueCount, 2); // only 2 unique names
    EXPECT_EQ(columns.nameIds[0], columns.nameIds[1]); // same UID for same name
    EXPECT_NE(columns.nameIds[0], columns.nameIds[2]);
}

TEST(SnapshotBuilderTest, CsrChildren) {
    // folder -> [file1, file2]
    std::vector<FileRecordInput> records = {
        MakeRecord(1, 0, "folder", FileRecordFlags::Directory),
        MakeRecord(2, 1, "file1.txt"),
        MakeRecord(3, 1, "file2.txt"),
    };
    auto columns = SnapshotBuilder::Build(records, MakeTestMeta());

    // folder is row 0 (id=1), its children should be rows 1 and 2
    EXPECT_EQ(columns.childStarts[0], 0);
    EXPECT_EQ(columns.childStarts[1], 2); // row 0 has 2 children
    EXPECT_EQ(columns.children.size(), 2);
    // Children should be row 1 and row 2
    std::vector<int32_t> childVals(columns.children.begin(), columns.children.end());
    std::sort(childVals.begin(), childVals.end());
    EXPECT_EQ(childVals[0], 1);
    EXPECT_EQ(childVals[1], 2);
}

TEST(SnapshotBuilderTest, UniqueMasks) {
    std::vector<FileRecordInput> records = {
        MakeRecord(1, 0, "abc"),
    };
    auto columns = SnapshotBuilder::Build(records, MakeTestMeta());

    ASSERT_EQ(columns.uniqueMasks.size(), 1);
    // 'a' = bit 0, 'b' = bit 1, 'c' = bit 2
    uint64_t expectedMask = (1ULL << 0) | (1ULL << 1) | (1ULL << 2);
    EXPECT_EQ(columns.uniqueMasks[0], expectedMask);
}

TEST(SnapshotBuilderTest, UniqueAsciiBits) {
    std::vector<FileRecordInput> records = {
        MakeRecord(1, 0, "ascii"),
        MakeRecord(2, 0, std::string("non") + '\xE2' + "scii"), // contains non-ASCII byte
    };
    auto columns = SnapshotBuilder::Build(records, MakeTestMeta());

    // UID 0 is ASCII, UID 1 is not
    ASSERT_GE(columns.uniqueAsciiBits.size(), 1);
    EXPECT_TRUE((columns.uniqueAsciiBits[0] >> 0) & 1); // UID 0 is ASCII
    EXPECT_FALSE((columns.uniqueAsciiBits[0] >> 1) & 1); // UID 1 is not ASCII
}

// --- SnapshotWriter round-trip ---

TEST(SnapshotWriterTest, WriteToStreamRoundTrip) {
    std::vector<FileRecordInput> records = {
        MakeRecord(1, 0, "root", FileRecordFlags::Directory),
        MakeRecord(2, 1, "file.txt"),
        MakeRecord(3, 1, "another.txt"),
    };
    auto meta = MakeTestMeta();

    auto columns = SnapshotBuilder::Build(records, meta);

    std::ostringstream s;
    SnapshotWriter::WriteToStream(columns, s);
    std::string data = s.str();

    // Verify magic at start.
    uint64_t magic;
    std::memcpy(&magic, data.data(), sizeof(magic));
    EXPECT_EQ(magic, kMagic);

    // Read header back.
    std::istringstream in(data);
    Meta readMeta = ReadHeader(in);
    EXPECT_EQ(readMeta.RowCount, 3);
    EXPECT_EQ(readMeta.SourceKey, "TestDrive");
}

TEST(SnapshotWriterTest, WriteToFileAndReadBack) {
    std::vector<FileRecordInput> records = {
        MakeRecord(10, 0, "folder", FileRecordFlags::Directory),
        MakeRecord(20, 10, "document.pdf", FileRecordFlags::None, 1024),
        MakeRecord(30, 10, "image.png", FileRecordFlags::None, 2048),
        MakeRecord(40, 0, "root_file.txt", FileRecordFlags::ReadOnly, 512),
    };
    auto meta = MakeTestMeta();

    // Write to a temp file.
    std::string tempPath = std::filesystem::temp_directory_path().string() + "/snapshot_test.bin";
    SnapshotWriter::Write(records, meta, tempPath);

    // Read back with Snapshot.
    Snapshot snap;
    snap.Open(tempPath);

    EXPECT_EQ(snap.RowCount(), 4);
    EXPECT_EQ(snap.GetMeta().RowCount, 4);
    EXPECT_EQ(snap.GetMeta().SourceKey, "TestDrive");

    // Check IDs are sorted.
    auto ids = snap.Ids();
    EXPECT_EQ(ids[0].low, 10);
    EXPECT_EQ(ids[1].low, 20);
    EXPECT_EQ(ids[2].low, 30);
    EXPECT_EQ(ids[3].low, 40);

    // Check names.
    EXPECT_EQ(snap.GetName(0), "folder");
    EXPECT_EQ(snap.GetName(1), "document.pdf");
    EXPECT_EQ(snap.GetName(2), "image.png");
    EXPECT_EQ(snap.GetName(3), "root_file.txt");

    // Check flags.
    EXPECT_TRUE(snap.IsDirectory(0));
    EXPECT_FALSE(snap.IsDirectory(1));
    EXPECT_TRUE(HasFlag(static_cast<FileRecordFlags>(snap.Flags()[3]), FileRecordFlags::ReadOnly));

    // Check sizes.
    auto sizes = snap.Sizes();
    EXPECT_EQ(sizes[1], 1024);
    EXPECT_EQ(sizes[2], 2048);
    EXPECT_EQ(sizes[3], 512);

    // Check parent resolution (folder is row 0, its children are rows 1, 2).
    auto children = snap.ChildrenOf(0);
    EXPECT_EQ(children.size(), 2);
    // Row 0 is "folder", children should be rows 1 and 2.
    std::vector<int32_t> childRows(children.begin(), children.end());
    std::sort(childRows.begin(), childRows.end());
    EXPECT_EQ(childRows[0], 1);
    EXPECT_EQ(childRows[1], 2);

    // Check UID lookup.
    EXPECT_EQ(snap.FindRowById({10, 0}), 0);
    EXPECT_EQ(snap.FindRowById({20, 0}), 1);
    EXPECT_EQ(snap.FindRowById({99, 0}), -1);

    // Clean up.
    std::filesystem::remove(tempPath);
}

TEST(SnapshotWriterTest, AtomicReplace) {
    std::string tempPath = std::filesystem::temp_directory_path().string() + "/snapshot_replace_test.bin";

    // Write first version.
    std::vector<FileRecordInput> v1 = { MakeRecord(1, 0, "old.txt") };
    SnapshotWriter::Write(v1, MakeTestMeta(), tempPath);

    // Read it.
    Snapshot snap1;
    snap1.Open(tempPath);
    EXPECT_EQ(snap1.RowCount(), 1);
    EXPECT_EQ(snap1.GetName(0), "old.txt");

    // Write second version (atomic replace).
    std::vector<FileRecordInput> v2 = {
        MakeRecord(1, 0, "new.txt"),
        MakeRecord(2, 0, "extra.txt"),
    };
    SnapshotWriter::Write(v2, MakeTestMeta(), tempPath);

    // Read it again (reopen to simulate new process).
    Snapshot snap2;
    snap2.Open(tempPath);
    EXPECT_EQ(snap2.RowCount(), 2);
    EXPECT_EQ(snap2.GetName(0), "new.txt");
    EXPECT_EQ(snap2.GetName(1), "extra.txt");

    std::filesystem::remove(tempPath);
}
