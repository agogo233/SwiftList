#include <gtest/gtest.h>

#include "app/app_config.h"

#include <filesystem>
#include <fstream>

namespace swiftlist::app::tests {
namespace {

class AppConfigTests : public ::testing::Test {
protected:
    void SetUp() override {
        tmpDir_ = std::filesystem::temp_path() / "swiftlist_test_config";
        std::filesystem::create_directories(tmpDir_);
        configPath_ = tmpDir_ / "config.ini";
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmpDir_, ec);
    }

    std::filesystem::path tmpDir_;
    std::filesystem::path configPath_;
};

TEST_F(AppConfigTests, DefaultValuesWhenFileMissing) {
    auto config = AppConfig::Load(configPath_.wstring());
    EXPECT_EQ(config.HotkeyModifiers, 2);  // MOD_CONTROL
    EXPECT_EQ(config.HotkeyKey, 32);        // VK_SPACE
    EXPECT_EQ(config.Theme, L"Dark");
    EXPECT_TRUE(config.IndexedDrives.empty());
}

TEST_F(AppConfigTests, SaveAndReload) {
    {
        AppConfig cfg;
        cfg.HotkeyModifiers = 4;  // MOD_SHIFT
        cfg.HotkeyKey = 65;       // 'A'
        cfg.Theme = L"Light";
        cfg.IndexedDrives = {L'C', L'D'};
        ASSERT_TRUE(cfg.Save(configPath_.wstring()));
    }

    auto loaded = AppConfig::Load(configPath_.wstring());
    EXPECT_EQ(loaded.HotkeyModifiers, 4);
    EXPECT_EQ(loaded.HotkeyKey, 65);
    EXPECT_EQ(loaded.Theme, L"Light");
    ASSERT_EQ(loaded.IndexedDrives.size(), 2);
    EXPECT_EQ(loaded.IndexedDrives[0], L'C');
    EXPECT_EQ(loaded.IndexedDrives[1], L'D');
}

TEST_F(AppConfigTests, UpdateExistingValue) {
    {
        AppConfig cfg;
        cfg.Save(configPath_.wstring());
    }
    {
        auto cfg = AppConfig::Load(configPath_.wstring());
        cfg.Theme = L"Light";
        cfg.Save(configPath_.wstring());
    }
    auto loaded = AppConfig::Load(configPath_.wstring());
    EXPECT_EQ(loaded.Theme, L"Light");
}

TEST_F(AppConfigTests, CorruptedFileFallsBackToDefaults) {
    {
        std::ofstream bad(configPath_);
        bad << "[General\nHotkeyModifiers=garbage\n";
        bad.close();
    }

    auto cfg = AppConfig::Load(configPath_.wstring());
    EXPECT_EQ(cfg.HotkeyModifiers, 2);
}

TEST_F(AppConfigTests, EmptyIniReturnsDefaults) {
    {
        std::ofstream empty(configPath_);
        empty.close();
    }

    auto cfg = AppConfig::Load(configPath_.wstring());
    EXPECT_EQ(cfg.HotkeyModifiers, 2);
    EXPECT_EQ(cfg.HotkeyKey, 32);
}

} // namespace
} // namespace swiftlist::app::tests
