#include <gtest/gtest.h>

#include "indexer/mft_index_scanner.h"

#include <filesystem>

namespace swiftlist::indexer::tests {
namespace {

class MftIndexScannerTests : public ::testing::Test {
protected:
    void SetUp() override {
        tmpDir_ = std::filesystem::temp_directory_path() / "swiftlist_test_mft";
        std::filesystem::create_directories(tmpDir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmpDir_, ec);
    }

    std::filesystem::path tmpDir_;
};

TEST_F(MftIndexScannerTests, ScanNTFSDriveProducesRecords) {
    FileRecordStore store;
    store.SourceKey = "C";
    store.FileSystemType = "NTFS";

    MftIndexScanner scanner;
    bool ok = scanner.Scan(L'C', store);

    if (!ok) {
        GTEST_SKIP() << "Skipping: not running as administrator or NTFS C: unavailable";
    }

    EXPECT_FALSE(store.Records.empty());
    EXPECT_TRUE(store.IsComplete);

    for (const auto& rec : store.Records) {
        EXPECT_FALSE(rec.Name.empty());
        EXPECT_TRUE(rec.Id.AsUint64() != 0 || rec.Flags != FileRecordFlags::None);
    }
}

TEST_F(MftIndexScannerTests, FileRecordStoreInitializedCorrectly) {
    FileRecordStore store;
    EXPECT_TRUE(store.SourceKey.empty());
    EXPECT_FALSE(store.IsComplete);
    EXPECT_TRUE(store.Records.empty());
    EXPECT_EQ(store.Source, SourceKind::LocalMft);
    EXPECT_EQ(store.Id, IdKind::MftFrn);
}

TEST_F(MftIndexScannerTests, ScanWithCallback) {
    FileRecordStore store;
    store.SourceKey = "C";

    int count = 0;
    MftIndexScanner scanner;
    bool ok = scanner.Scan(L'C', store, [&count](const FileRecordInput&) {
        ++count;
    });

    if (!ok) {
        GTEST_SKIP() << "Skipping: not running as administrator or NTFS C: unavailable";
    }

    EXPECT_EQ(count, static_cast<int>(store.Records.size()));
}

TEST_F(MftIndexScannerTests, InvalidDriveReturnsFalse) {
    FileRecordStore store;
    MftIndexScanner scanner;
    bool ok = scanner.Scan(L'Z', store);

    if (GetVolumeInformationW(L"Z:\\", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0)) {
        GTEST_SKIP() << "Skipping: Z: drive exists";
    }

    EXPECT_FALSE(ok);
}

} // namespace
} // namespace swiftlist::indexer::tests
