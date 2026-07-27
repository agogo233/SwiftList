#pragma once

#include "pipe/wire_format.h"

#include <cstdint>
#include <span>
#include <vector>

namespace swiftlist::pipe {

inline constexpr uint16_t kHookIpcMagic = 0x4B48;  // "HK"
inline constexpr uint8_t kHookIpcVersion = 1;

enum class HookEventType : uint8_t {
    DoubleCtrl = 0x01,
    MouseClick = 0x02,
    ExplorerPathChanged = 0x03,
    SearchVisible = 0x04,
    SearchFinished = 0x05,
};

inline void WriteHookEvent(std::vector<uint8_t>& buf, HookEventType type, uint32_t data) {
    buf.resize(8);
    buf[0] = static_cast<uint8_t>(kHookIpcMagic & 0xFF);
    buf[1] = static_cast<uint8_t>((kHookIpcMagic >> 8) & 0xFF);
    buf[2] = kHookIpcVersion;
    buf[3] = static_cast<uint8_t>(type);
    WriteUInt32LE(std::span<uint8_t>(buf.data() + 4, 4), data);
}

inline bool ReadHookEvent(const uint8_t* data, size_t len,
                           HookEventType& type, uint32_t& outData) {
    if (len < 8) return false;

    uint16_t magic = static_cast<uint16_t>(data[0]) |
                     (static_cast<uint16_t>(data[1]) << 8);
    if (magic != kHookIpcMagic) return false;

    if (data[2] != kHookIpcVersion) return false;

    type = static_cast<HookEventType>(data[3]);
    size_t offset = 4;
    outData = ReadUInt32LE(std::span<const uint8_t>(data, len), offset);
    return true;
}

} // namespace swiftlist::pipe
