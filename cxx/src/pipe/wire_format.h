#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace swiftlist::pipe {

constexpr uint32_t kRequestMagic = 0x51504C53;   // SLPQ
constexpr uint32_t kResponseMagic = 0x52504C53;  // SLPR
constexpr uint32_t kSearchResMagic = 0x53524C53;  // SLRS
constexpr uint32_t kIpcMagic = 0x51504C53;        // SLPQ (same as request, version disambiguates)

constexpr int kRequestVersion = 4;
constexpr int kResponseVersion = 4;
constexpr int kSearchResVersion = 4;
constexpr int kIpcVersion = 2;
constexpr int kStringVersion = 1;

constexpr size_t kMaxPayloadSize = 10 * 1024 * 1024;

struct VarIntResult {
    int bytesWritten;
    bool overflow;
};

inline VarIntResult Write7BitEncodedInt(std::span<uint8_t> dest, int32_t value) {
    uint32_t u = static_cast<uint32_t>(value);
    int count = 0;
    while (u >= 0x80) {
        if (count >= static_cast<int>(dest.size())) return {count, true};
        dest[count++] = static_cast<uint8_t>(u | 0x80);
        u >>= 7;
    }
    if (count >= static_cast<int>(dest.size())) return {count, true};
    dest[count++] = static_cast<uint8_t>(u);
    return {count, false};
}

inline int32_t Read7BitEncodedInt(std::span<const uint8_t> src, size_t& offset) {
    uint32_t result = 0;
    int shift = 0;
    while (shift < 35) {
        if (offset >= src.size()) throw std::runtime_error("varint overrun");
        uint8_t b = src[offset++];
        result |= static_cast<uint32_t>(b & 0x7F) << shift;
        shift += 7;
        if ((b & 0x80) == 0) return static_cast<int32_t>(result);
    }
    throw std::runtime_error("varint too long");
}

inline void WriteString(std::vector<uint8_t>& buf, std::string_view str) {
    auto len = static_cast<int32_t>(str.size());
    auto prev = buf.size();
    auto viLen = Max7BitLen(len);
    buf.resize(prev + viLen + str.size());
    auto r = Write7BitEncodedInt(std::span<uint8_t>(buf.data() + prev, viLen), len);
    if (r.bytesWritten < static_cast<int>(viLen)) {
        std::memmove(buf.data() + prev + r.bytesWritten,
                     buf.data() + prev + viLen,
                     str.size());
        buf.resize(prev + r.bytesWritten + str.size());
    }
    std::memcpy(buf.data() + buf.size() - str.size(), str.data(), str.size());
}

inline std::string ReadString(std::span<const uint8_t> src, size_t& offset) {
    int32_t len = Read7BitEncodedInt(src, offset);
    if (len <= 0) return {};
    if (offset + static_cast<size_t>(len) > src.size()) throw std::runtime_error("string overrun");
    std::string s(reinterpret_cast<const char*>(src.data() + offset), static_cast<size_t>(len));
    offset += static_cast<size_t>(len);
    return s;
}

constexpr size_t Max7BitLen(int32_t value) {
    uint32_t u = static_cast<uint32_t>(value);
    size_t n = 1;
    while (u >= 0x80) { u >>= 7; ++n; }
    return n;
}

inline void WriteInt32LE(std::span<uint8_t> dest, int32_t value) {
    dest[0] = static_cast<uint8_t>(value);
    dest[1] = static_cast<uint8_t>(value >> 8);
    dest[2] = static_cast<uint8_t>(value >> 16);
    dest[3] = static_cast<uint8_t>(value >> 24);
}

inline int32_t ReadInt32LE(std::span<const uint8_t> src, size_t& offset) {
    int32_t v = static_cast<int32_t>(src[offset]) |
                 (static_cast<int32_t>(src[offset + 1]) << 8) |
                 (static_cast<int32_t>(src[offset + 2]) << 16) |
                 (static_cast<int32_t>(src[offset + 3]) << 24);
    offset += 4;
    return v;
}

inline void WriteUInt64LE(std::span<uint8_t> dest, uint64_t value) {
    for (int i = 0; i < 8; ++i) dest[i] = static_cast<uint8_t>(value >> (i * 8));
}

inline uint64_t ReadUInt64LE(std::span<const uint8_t> src, size_t& offset) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(src[offset + i]) << (i * 8);
    offset += 8;
    return v;
}

inline void WriteInt64LE(std::span<uint8_t> dest, int64_t value) {
    WriteUInt64LE(dest, static_cast<uint64_t>(value));
}

inline int64_t ReadInt64LE(std::span<const uint8_t> src, size_t& offset) {
    return static_cast<int64_t>(ReadUInt64LE(src, offset));
}

inline void WriteUInt32LE(std::span<uint8_t> dest, uint32_t value) {
    dest[0] = static_cast<uint8_t>(value);
    dest[1] = static_cast<uint8_t>(value >> 8);
    dest[2] = static_cast<uint8_t>(value >> 16);
    dest[3] = static_cast<uint8_t>(value >> 24);
}

inline uint32_t ReadUInt32LE(std::span<const uint8_t> src, size_t& offset) {
    uint32_t v = static_cast<uint32_t>(src[offset]) |
                  (static_cast<uint32_t>(src[offset + 1]) << 8) |
                  (static_cast<uint32_t>(src[offset + 2]) << 16) |
                  (static_cast<uint32_t>(src[offset + 3]) << 24);
    offset += 4;
    return v;
}

} // namespace swiftlist::pipe
