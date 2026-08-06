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
        size_t so = 0;
        result.push_back(ReadString(std::span<const uint8_t>(payload + offset, end - offset), so));
        offset += so;
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
        size_t so = 0;
        settings.LocalDrives.push_back(ReadString(
            std::span<const uint8_t>(payload + offset, end - offset), so));
        offset += so;
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
        WriteStringList(buf, msg.DisabledAliasComponents.get());
        buf.push_back(msg.ExactMatch ? 1 : 0);
        break;
    }
    case SearchRequestId::SearchDir: {
        auto o = buf.size();
        buf.resize(o + 8);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o, 4), msg.Limit);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o + 4, 4), msg.AppLimit);
        WriteString(buf, msg.DirectoryFilter);
        WriteString(buf, msg.Query);
        WriteStringList(buf, msg.DisabledAliasComponents.get());
        buf.push_back(msg.ExactMatch ? 1 : 0);
        break;
    }
    case SearchRequestId::GetFileMetadata:
        WriteStringList(buf, msg.FilePaths.get());
        break;
    case SearchRequestId::GetRecentFiles: {
        auto o = buf.size();
        buf.resize(o + 8);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o, 4), msg.Limit);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o + 4, 4), msg.MaxAgeMinutes);
        WriteStringList(buf, msg.Directories.get());
        break;
    }
    case SearchRequestId::LaunchHook:
        buf.push_back(msg.RequestElevation ? 1 : 0);
        break;
    case SearchRequestId::CancelDriveIndex:
        WriteString(buf, msg.Drive);
        break;
    case SearchRequestId::EnumerateDir: {
        auto o = buf.size();
        buf.resize(o + 4);
        WriteInt32LE(std::span<uint8_t>(buf.data() + o, 4), msg.Limit);
        WriteString(buf, msg.DirectoryFilter);
        WriteString(buf, msg.Query);
        buf.push_back(msg.Recursive ? 1 : 0);
        break;
    }
    case SearchRequestId::SubscribeDirectoryChanges:
        WriteStringList(buf, msg.Directories.get());
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
        msg.MachineSettingsPtr = std::make_unique<MachineSettings>(
            ReadMachineSettings(payload, offset, len));
        break;
    case SearchRequestId::RebuildDrive:
    case SearchRequestId::DeleteDriveIndex: {
        size_t so = 0;
        msg.Drive = ReadString(std::span<const uint8_t>(payload + offset, len - offset), so);
        offset += so;
        break;
    }
    case SearchRequestId::Search: {
        if (offset + 8 > len) return false;
        msg.Limit = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
        msg.AppLimit = ReadInt32LE(std::span<const uint8_t>(payload + offset + 4, 4));
        offset += 8;
        size_t so = 0;
        msg.Query = ReadString(std::span<const uint8_t>(payload + offset, len - offset), so);
        offset += so;
        msg.DisabledAliasComponents = std::make_unique<std::vector<std::string>>(
            ReadStringList(payload, offset, len));
        if (offset >= len) return false;
        msg.ExactMatch = payload[offset++] != 0;
        break;
    }
    case SearchRequestId::SearchDir: {
        if (offset + 8 > len) return false;
        msg.Limit = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
        msg.AppLimit = ReadInt32LE(std::span<const uint8_t>(payload + offset + 4, 4));
        offset += 8;
        size_t so = 0;
        msg.DirectoryFilter = ReadString(std::span<const uint8_t>(payload + offset, len - offset), so);
        offset += so;
        so = 0;
        msg.Query = ReadString(std::span<const uint8_t>(payload + offset, len - offset), so);
        offset += so;
        msg.DisabledAliasComponents = std::make_unique<std::vector<std::string>>(
            ReadStringList(payload, offset, len));
        if (offset >= len) return false;
        msg.ExactMatch = payload[offset++] != 0;
        break;
    }
    case SearchRequestId::GetFileMetadata:
        msg.FilePaths = std::make_unique<std::vector<std::string>>(
            ReadStringList(payload, offset, len));
        break;
    case SearchRequestId::GetRecentFiles:
        if (offset + 8 > len) return false;
        msg.Limit = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
        msg.MaxAgeMinutes = ReadInt32LE(std::span<const uint8_t>(payload + offset + 4, 4));
        offset += 8;
        msg.Directories = std::make_unique<std::vector<std::string>>(
            ReadStringList(payload, offset, len));
        break;
    case SearchRequestId::LaunchHook:
        if (offset >= len) return false;
        msg.RequestElevation = payload[offset++] != 0;
        break;
    case SearchRequestId::CancelDriveIndex: {
        size_t so = 0;
        msg.Drive = ReadString(std::span<const uint8_t>(payload + offset, len - offset), so);
        offset += so;
        break;
    }
    case SearchRequestId::EnumerateDir: {
        if (offset + 4 > len) return false;
        msg.Limit = ReadInt32LE(std::span<const uint8_t>(payload + offset, 4));
        offset += 4;
        size_t so = 0;
        msg.DirectoryFilter = ReadString(std::span<const uint8_t>(payload + offset, len - offset), so);
        offset += so;
        so = 0;
        msg.Query = ReadString(std::span<const uint8_t>(payload + offset, len - offset), so);
        offset += so;
        if (offset >= len) return false;
        msg.Recursive = payload[offset++] != 0;
        break;
    }
    case SearchRequestId::SubscribeDirectoryChanges:
        msg.Directories = std::make_unique<std::vector<std::string>>(
            ReadStringList(payload, offset, len));
        break;
    default:
        break;
    }

    return true;
}

} // namespace swiftlist::pipe
