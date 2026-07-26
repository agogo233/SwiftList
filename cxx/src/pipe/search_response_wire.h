#pragma once

#include "pipe/wire_format.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace swiftlist::pipe {

struct FileMetadata {
    int64_t Size = 0;
    uint32_t CreatedUnix = 0;
    uint32_t ModifiedUnix = 0;
    uint32_t AccessedUnix = 0;
};

struct SearchResult {
    std::string Name;
    std::string Path;
    bool IsDir = false;
    std::string Drive;
    uint64_t RankSortKey = 0;
    FileMetadata Metadata;
};

static constexpr uint8_t kEndFrame = 0;
static constexpr uint8_t kFileResultFrame = 1;
static constexpr uint8_t kAppResultFrame = 2;
static constexpr uint8_t kHeaderFrame = 255;

void WriteSearchResponseHeader(std::vector<uint8_t>& buf);

void WriteSearchResponseFileResult(std::vector<uint8_t>& buf,
                                    const SearchResult& result);

void WriteSearchResponseEnd(std::vector<uint8_t>& buf);

bool ReadSearchResponseStream(
    const uint8_t* data, size_t len,
    std::function<void(const SearchResult&)> onResult);

} // namespace swiftlist::pipe
