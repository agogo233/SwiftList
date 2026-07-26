#include "pipe/search_request_wire.h"

namespace swiftlist::pipe {

static void WriteStringList(std::vector<uint8_t>& buf,
                            const std::vector<std::string>* list) {
    int32_t count = list ? static_cast<int32_t>(list->size()) : 0;
    auto off = buf.size();
    buf.resize(off + 4);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off, 4), count);
    if (list) {
        for (const auto& s : *list) WriteString(buf, s);
    }
}

static std::vector<std::string> ReadStringList(const uint8_t* payload,
                                                size_t& offset, size_t end) {
    if (offset + 4 > end) return {};
    int32_t count = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
    offset += 4;
    std::vector<std::string> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (offset >= end) break;
        result.push_back(ReadString(std::span<const uint8_t>(payload + offset, end - offset), offset));
    }
    return result;
}

static void WriteMachineSettings(std::vector<uint8_t>& buf,
                                 const MachineSettings& settings) {
    int32_t count = static_cast<int32_t>(settings.LocalDrives.size());
    auto off = buf.size();
    buf.resize(off + 4);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off, 4), count);
    for (const auto& d : settings.LocalDrives) WriteString(buf, d);
}

static MachineSettings ReadMachineSettings(const uint8_t* payload,
                                            size_t& offset, size_t end) {
    MachineSettings settings;
    if (offset + 4 > end) return settings;
    int32_t count = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
    offset += 4;
    for (int i = 0; i < count; ++i) {
        if (offset >= end) break;
        settings.LocalDrives.push_back(ReadString(
            std::span<const uint8_t>(payload + offset, end - offset), offset));
    }
    return settings;
}

void WriteSearchRequest(std::vector<uint8_t>& buf,
                         const SearchRequestMessage& msg) {
    auto off = buf.size();
    buf.resize(off + 12);
    auto payloadStart = off + 12;
    buf.push_back(static_cast<uint8_t>(msg.Id));

    switch (msg.Id) {
    case SearchRequestId::SetMachineSettings:
        WriteMachineSettings(buf, msg.MachineSettingsPtr ? *msg.MachineSettingsPtr
                                                          : MachineSettings{});
        break;
    case SearchRequestId::RebuildDrive:
    case SearchRequestId::DeleteDriveIndex:
        WriteString(buf, msg.Drive);
        break;
    case SearchRequestId::Search: {
        auto o = buf.size();
        buf.resize(o + 8);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o, 4), msg.Limit);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o + 4, 4), msg.AppLimit);
        WriteString(buf, msg.Query);
        WriteStringList(buf, msg.DisabledAliasComponents);
        break;
    }
    case SearchRequestId::SearchDir: {
        auto o = buf.size();
        buf.resize(o + 8);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o, 4), msg.Limit);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o + 4, 4), msg.AppLimit);
        WriteString(buf, msg.DirectoryFilter);
        WriteString(buf, msg.Query);
        WriteStringList(buf, msg.DisabledAliasComponents);
        break;
    }
    case SearchRequestId::GetFileMetadata:
        WriteStringList(buf, msg.FilePaths);
        break;
    case SearchRequestId::GetRecentFiles: {
        auto o = buf.size();
        buf.resize(o + 8);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o, 4), msg.Limit);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o + 4, 4), msg.MaxAgeMinutes);
        WriteStringList(buf, msg.Directories);
        break;
    }
    case SearchRequestId::LaunchHook:
        buf.push_back(msg.RequestElevation ? 1 : 0);
        break;
    default:
        break;
    }

    auto payloadSize = static_cast<int32_t>(buf.size() - payloadStart);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off, 4), kRequestMagic);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off + 4, 4), kRequestVersion);
    WriteInt32LE(std::span<uint8_t>(buf.data() + off + 8, 4), payloadSize);
}

bool ReadSearchRequest(const uint8_t* payload, size_t len,
                        SearchRequestMessage& msg) {
    if (len < 1) return false;
    size_t offset = 0;
    msg.Id = static_cast<SearchRequestId>(payload[offset++]);

    switch (msg.Id) {
    case SearchRequestId::SetMachineSettings:
        msg.MachineSettingsPtr = new MachineSettings(
            ReadMachineSettings(payload, offset, len));
        break;
    case SearchRequestId::RebuildDrive:
    case SearchRequestId::DeleteDriveIndex:
        msg.Drive = ReadString(std::span<const uint8_t>(payload + offset, len - offset), offset);
        break;
    case SearchRequestId::Search:
        if (offset + 8 > len) return false;
        msg.Limit = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
        msg.AppLimit = ReadInt32LE(std::span<const uint8_t>(payload + offset + 4, 4));
        offset += 8;
        msg.Query = ReadString(std::span<const uint8_t>(payload + offset, len - offset), offset);
        msg.DisabledAliasComponents = new std::vector<std::string>(
            ReadStringList(payload, offset, len));
        break;
    case SearchRequestId::SearchDir:
        if (offset + 8 > len) return false;
        msg.Limit = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
        msg.AppLimit = ReadInt32LE(std::span<const uint8_t>(payload + offset + 4, 4));
        offset += 8;
        msg.DirectoryFilter = ReadString(std::span<const uint8_t>(payload + offset, len - offset), offset);
        msg.Query = ReadString(std::span<const uint8_t>(payload + offset, len - offset), offset);
        msg.DisabledAliasComponents = new std::vector<std::string>(
            ReadStringList(payload, offset, len));
        break;
    case SearchRequestId::GetFileMetadata:
        msg.FilePaths = new std::vector<std::string>(
            ReadStringList(payload, offset, len));
        break;
    case SearchRequestId::GetRecentFiles:
        if (offset + 8 > len) return false;
        msg.Limit = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
        msg.MaxAgeMinutes = ReadInt32LE(std::span<const uint8_t>(payload + offset + 4, 4));
        offset += 8;
        msg.Directories = new std::vector<std::string>(
            ReadStringList(payload, offset, len));
        break;
    case SearchRequestId::LaunchHook:
        if (offset >= len) return false;
        msg.RequestElevation = payload[offset++] != 0;
        break;
    default:
        break;
    }

    return true;
}

} // namespace swiftlist::pipe
