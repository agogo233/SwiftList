#pragma once

#include "pipe/pipe_client.h"
#include "pipe/search_response_wire.h"

#include <functional>
#include <string>
#include <vector>

namespace swiftlist::cli {

struct CliSearchResult {
    std::wstring Name;
    std::wstring Path;
    bool IsDir = false;
    std::wstring Drive;
    uint64_t RankSortKey = 0;
};

class AppSearchPipeClient {
public:
    using ResultFn = std::function<void(const CliSearchResult&)>;

    AppSearchPipeClient() = default;
    ~AppSearchPipeClient() = default;

    AppSearchPipeClient(const AppSearchPipeClient&) = delete;
    AppSearchPipeClient& operator=(const AppSearchPipeClient&) = delete;

    bool Connect(const std::wstring& pipeName);
    void Disconnect();

    bool Search(const std::wstring& query, int limit,
                ResultFn onResult);

private:
    static std::wstring ToWString(const std::string& s);
    static std::string ToUtf8(const std::wstring& s);

    pipe::PipeClient client_;
};

} // namespace swiftlist::cli
