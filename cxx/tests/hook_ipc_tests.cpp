#include <gtest/gtest.h>

#include "pipe/hook_ipc_message.h"

#include "pipe/wire_format.h"

#include <cstring>

namespace swiftlist::pipe::tests {
namespace {

TEST(HookIpcMessageTests, SerializeDoubleCtrl) {
    std::vector<uint8_t> buf;
    WriteHookEvent(buf, HookEventType::DoubleCtrl, 0);

    EXPECT_GE(buf.size(), 4u);
    EXPECT_EQ(buf[0], kHookIpcMagic & 0xFF);
    EXPECT_EQ(buf[1], (kHookIpcMagic >> 8) & 0xFF);
    EXPECT_EQ(buf[2], kHookIpcVersion);
    EXPECT_EQ(buf[3], static_cast<uint8_t>(HookEventType::DoubleCtrl));
}

TEST(HookIpcMessageTests, SerializeMouseClick) {
    std::vector<uint8_t> buf;
    constexpr uint32_t kMouseData = (100 << 16) | 200;
    WriteHookEvent(buf, HookEventType::MouseClick, kMouseData);

    ASSERT_GE(buf.size(), 8u);
    EXPECT_EQ(buf[3], static_cast<uint8_t>(HookEventType::MouseClick));

    size_t offset = 4;
    uint32_t data = ReadUInt32LE(buf, offset);
    EXPECT_EQ(data, kMouseData);
}

TEST(HookIpcMessageTests, SerializeExplorerPath) {
    std::vector<uint8_t> buf;
    WriteHookEvent(buf, HookEventType::ExplorerPathChanged, 0xDEADBEEF);

    EXPECT_GE(buf.size(), 8u);
    EXPECT_EQ(buf[3], static_cast<uint8_t>(HookEventType::ExplorerPathChanged));
}

TEST(HookIpcMessageTests, RoundTrip) {
    std::vector<uint8_t> buf;
    WriteHookEvent(buf, HookEventType::MouseClick, 0x12345678);

    HookEventType type = HookEventType::None;
    uint32_t data = 0;
    ASSERT_TRUE(ReadHookEvent(buf.data(), buf.size(), type, data));

    EXPECT_EQ(type, HookEventType::MouseClick);
    EXPECT_EQ(data, 0x12345678);
}

TEST(HookIpcMessageTests, ReadInvalidMagic) {
    std::vector<uint8_t> buf = {0xFF, 0xFF, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00};

    HookEventType type = HookEventType::None;
    uint32_t data = 0;
    EXPECT_FALSE(ReadHookEvent(buf.data(), buf.size(), type, data));
}

TEST(HookIpcMessageTests, ReadTruncatedBuffer) {
    std::vector<uint8_t> buf = {0x48, 0x4B};  // partial magic only

    HookEventType type = HookEventType::None;
    uint32_t data = 0;
    EXPECT_FALSE(ReadHookEvent(buf.data(), buf.size(), type, data));
}

} // namespace
} // namespace swiftlist::pipe::tests
