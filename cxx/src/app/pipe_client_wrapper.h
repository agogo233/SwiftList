#pragma once

#include "pipe/pipe_client.h"
#include "pipe/search_request_wire.h"
#include "pipe/search_response_wire.h"

#include <mutex>
#include <string>
#include <vector>

namespace swiftlist::app {

struct AppSearchResult {
    std::wstring Name;
    std::wstring Path;
    bool IsDir = false;
    std::wstring Drive;
    uint64_t RankSortKey = 0;
    pipe::FileMetadata Metadata;
};

class PipeClientWrapper {
public:
    PipeClientWrapper() = default;
    ~PipeClientWrapper();

    PipeClientWrapper(const PipeClientWrapper&) = delete;
    PipeClientWrapper& operator=(const PipeClientWrapper&) = delete;

    bool Connect(const std::wstring& pipeName);
    void Disconnect();
    bool IsConnected() const;

    bool SendSearch(const std::wstring& query, int limit);

    const std::vector<AppSearchResult>& LastResults() const { return lastResults_; }

private:
    static std::wstring ToWString(const std::string& s);

    pipe::PipeClient client_;
    std::mutex mutex_;
    std::vector<AppSearchResult> lastResults_;
};

} // namespace swiftlist::app
