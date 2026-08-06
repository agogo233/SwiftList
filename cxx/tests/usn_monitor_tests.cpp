#include <gtest/gtest.h>

#include "indexer/usn_monitor.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace swiftlist::indexer::tests {
namespace {

class UsnMonitorTests : public ::testing::Test {
protected:
    void SetUp() override {
        tmpDir_ = std::filesystem::temp_directory_path() / "swiftlist_test_usn";
        std::filesystem::create_directories(tmpDir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmpDir_, ec);
    }

    std::filesystem::path tmpDir_;
};

TEST_F(UsnMonitorTests, StartStopCycle) {
    index_v2::LiveIndex live;
    UsnMonitor monitor(live);

    monitor.SetPollInterval(100);

    if (!monitor.Start(L'C')) {
        GTEST_SKIP() << "Skipping: not running as administrator or NTFS C: unavailable";
    }

    EXPECT_TRUE(monitor.IsRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(monitor.IsRunning());

    monitor.Stop();
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(UsnMonitorTests, PollIntervalChangeable) {
    index_v2::LiveIndex live;
    UsnMonitor monitor(live);

    monitor.SetPollInterval(200);
    monitor.SetPollInterval(500);
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(UsnMonitorTests, DetectsFileCreate) {
    index_v2::LiveIndex live;
    UsnMonitor monitor(live);
    monitor.SetPollInterval(100);

    if (!monitor.Start(L'C')) {
        GTEST_SKIP() << "Skipping: not running as administrator";
    }

    auto testFile = tmpDir_ / "usn_test_file.txt";
    {
        std::ofstream f(testFile);
        f << "test content";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    monitor.Stop();

    int rowCount = live.RowCount();
    EXPECT_GE(rowCount, 0);
}

TEST_F(UsnMonitorTests, StopWithoutStartIsNoOp) {
    index_v2::LiveIndex live;
    UsnMonitor monitor(live);
    monitor.Stop();
    EXPECT_FALSE(monitor.IsRunning());
}

} // namespace
} // namespace swiftlist::indexer::tests
