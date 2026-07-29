#pragma once

#include "pipe/wire_format.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace swiftlist::pipe {

enum class SearchRequestId : uint8_t {
    Ping = 0,
    Status = 1,
    Rebuild = 2,
    GetMachineSettings = 3,
    SetMachineSettings = 4,
    Search = 5,
    SearchDir = 6,
    RebuildDrive = 7,
    DeleteDriveIndex = 8,
    SubscribeStatus = 9,
    Initialize = 10,
    GetFileMetadata = 11,
    ClearServiceLog = 12,
    GetRecentFiles = 13,
    ClearPathCaches = 14,
    LaunchHook = 15,
};

struct MachineSettings {
    std::vector<std::string> LocalDrives;
};

struct SearchRequestMessage {
    SearchRequestId Id = SearchRequestId::Ping;
    int32_t Limit = 0;
    int32_t AppLimit = 0;
    std::string Query;
    std::string DirectoryFilter;
    std::string Drive;
    std::unique_ptr<MachineSettings> MachineSettingsPtr;
    std::unique_ptr<std::vector<std::string>> DisabledAliasComponents;
    std::unique_ptr<std::vector<std::string>> FilePaths;
    std::unique_ptr<std::vector<std::string>> Directories;
    int32_t MaxAgeMinutes = 0;
    bool RequestElevation = false;
};

void WriteSearchRequest(std::vector<uint8_t>& buf,
                         const SearchRequestMessage& msg);

bool ReadSearchRequest(const uint8_t* payload, size_t len,
                        SearchRequestMessage& msg);

} // namespace swiftlist::pipe
