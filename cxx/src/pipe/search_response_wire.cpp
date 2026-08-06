#include "pipe/search_response_wire.h"

namespace swiftlist::pipe {

void WriteSearchResponseHeader(std::vector<uint8_t>& buf) {
    auto off = buf.size();
    buf.resize(off + 13);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off, 4), kSearchResMagic);
    buf[off + 4] = kHeaderFrame;
    WriteInt32LE(std::span<uint8_t>(buf.data() + off + 5, 4), 4);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off + 9, 4), kSearchResVersion);
}

void WriteSearchResponseFileResult(std::vector<uint8_t>& buf,
                                    const SearchResult& result) {
    auto nameLen = static_cast<int32_t>(result.Name.size());
    auto pathLen = static_cast<int32_t>(result.Path.size());
    auto driveLen = static_cast<int32_t>(result.Drive.size());

    auto payloadLen = Max7BitLen(nameLen) + nameLen +
                      Max7BitLen(pathLen) + pathLen +
                      1 +
                      Max7BitLen(driveLen) + driveLen +
                      8 + 8 + 4 + 4 + 4 + 4;

    auto totalFrameSize = 9 + payloadLen;
    auto off = buf.size();
    buf.resize(off + totalFrameSize);

    WriteInt32LE(std::span<uint8_t>(buf.data() + off, 4), kSearchResMagic);
    buf[off + 4] = kFileResultFrame;

    auto payloadStart = off + 9;
    auto offset = payloadStart;

    auto r = Write7BitEncodedInt(std::span<uint8_t>(buf.data() + offset, Max7BitLen(nameLen)), nameLen);
    offset += r.bytesWritten;
    std::memcpy(buf.data() + offset, result.Name.data(), nameLen);
    offset += nameLen;

    r = Write7BitEncodedInt(std::span<uint8_t>(buf.data() + offset, Max7BitLen(pathLen)), pathLen);
    offset += r.bytesWritten;
    std::memcpy(buf.data() + offset, result.Path.data(), pathLen);
    offset += pathLen;

    buf[offset++] = result.IsDir ? 1 : 0;

    r = Write7BitEncodedInt(std::span<uint8_t>(buf.data() + offset, Max7BitLen(driveLen)), driveLen);
    offset += r.bytesWritten;
    std::memcpy(buf.data() + offset, result.Drive.data(), driveLen);
    offset += driveLen;

    WriteUInt64LE(std::span<uint8_t>(buf.data() + offset, 8), result.RankSortKey);
    offset += 8;
    WriteInt64LE(std::span<uint8_t>(buf.data() + offset, 8), result.Metadata.Size);
    offset += 8;
    WriteUInt32LE(std::span<uint8_t>(buf.data() + offset, 4), result.Metadata.CreatedUnix);
    offset += 4;
    WriteUInt32LE(std::span<uint8_t>(buf.data() + offset, 4), result.Metadata.ModifiedUnix);
    offset += 4;
    WriteUInt32LE(std::span<uint8_t>(buf.data() + offset, 4), result.Metadata.AccessedUnix);
    offset += 4;

    WriteUInt32LE(std::span<uint8_t>(buf.data() + offset, 4), result.Attributes);
    offset += 4;

    auto actualPayloadSize = static_cast<int32_t>(offset - payloadStart);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off + 5, 4), actualPayloadSize);

    buf.resize(offset);
}

void WriteSearchResponseEnd(std::vector<uint8_t>& buf) {
    auto off = buf.size();
    buf.resize(off + 9);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off, 4), kSearchResMagic);
    buf[off + 4] = kEndFrame;
    WriteInt32LE(std::span<uint8_t>(buf.data() + off + 5, 4), 0);
}

void WriteSearchResponseNotIndexed(std::vector<uint8_t>& buf) {
    auto off = buf.size();
    buf.resize(off + 9);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off, 4), kSearchResMagic);
    buf[off + 4] = kNotIndexedFrame;
    WriteInt32LE(std::span<uint8_t>(buf.data() + off + 5, 4), 0);
}

bool ReadSearchResponseStream(
    const uint8_t* data, size_t len,
    std::function<void(const SearchResult&)> onResult) {
    size_t offset = 0;
    while (offset < len) {
        if (offset + 9 > len) return false;

        auto magic = ReadUInt32LE(std::span<const uint8_t>(data + offset, 4));
        if (magic != kSearchResMagic) return false;

        uint8_t frameType = data[offset + 4];
        auto payloadLen = ReadUInt32LE(std::span<const uint8_t>(data + offset + 5, 4));
        offset += 9;

        if (payloadLen > kMaxPayloadSize) return false;
        if (offset + payloadLen > len) return false;

        if (frameType == kEndFrame) return true;

        if (frameType == kHeaderFrame) {
            if (payloadLen < 4) return false;
            auto version = ReadUInt32LE(std::span<const uint8_t>(data + offset, 4));
            if (version != kSearchResVersion) return false;
            offset += payloadLen;
            continue;
        }

        if (frameType == kFileResultFrame || frameType == kAppResultFrame) {
            size_t poff = 0;
            auto payload = std::span<const uint8_t>(data + offset, payloadLen);
            SearchResult result;
            result.Name = ReadString(payload, poff);
            result.Path = ReadString(payload, poff);
            if (poff >= payloadLen) return false;
            result.IsDir = payload[poff++] != 0;
            result.Drive = ReadString(payload, poff);
            if (poff + 8 > payloadLen) return false;
            result.RankSortKey = ReadUInt64LE(payload.subspan(poff));
            poff += 8;
            if (poff + 8 > payloadLen) return false;
            result.Metadata.Size = ReadInt64LE(payload.subspan(poff));
            poff += 8;
            if (poff + 12 > payloadLen) return false;
            result.Metadata.CreatedUnix = ReadUInt32LE(payload.subspan(poff));
            result.Metadata.ModifiedUnix = ReadUInt32LE(payload.subspan(poff + 4));
            result.Metadata.AccessedUnix = ReadUInt32LE(payload.subspan(poff + 8));
            poff += 12;
            if (poff + 4 <= payloadLen) {
                result.Attributes = ReadUInt32LE(payload.subspan(poff));
            }
            onResult(result);
        }

        if (frameType == kNotIndexedFrame) {
            offset += payloadLen;
            continue;
        }

        offset += payloadLen;
    }
    return true;
}

} // namespace swiftlist::pipe
