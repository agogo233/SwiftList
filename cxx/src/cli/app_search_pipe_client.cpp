#include "cli/app_search_pipe_client.h"

#include <WinNls.h>

namespace swiftlist::cli {

std::wstring AppSearchPipeClient::ToWString(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                   static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                         static_cast<int>(s.size()), result.data(), len);
    return result;
}

std::string AppSearchPipeClient::ToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(),
                                   static_cast<int>(s.size()),
                                   nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(),
                         static_cast<int>(s.size()),
                         result.data(), len, nullptr, nullptr);
    return result;
}

bool AppSearchPipeClient::Connect(const std::wstring& pipeName) {
    return client_.Connect(pipeName);
}

void AppSearchPipeClient::Disconnect() {
    if (client_.IsConnected()) {
        client_.Disconnect();
    }
}

bool AppSearchPipeClient::Search(const std::wstring& query, int limit,
                                  ResultFn onResult) {
    if (!client_.IsConnected()) return false;

    std::string queryUtf8 = ToUtf8(query);

    std::vector<uint8_t> requestBuf;
    pipe::SearchRequestMessage req;
    req.Id = pipe::SearchRequestId::Search;
    req.Query = queryUtf8;
    req.Limit = limit;
    req.AppLimit = limit;
    pipe::WriteSearchRequest(requestBuf, req);

    if (!client_.WriteAll(requestBuf.data(),
                           static_cast<DWORD>(requestBuf.size()))) {
        return false;
    }

    std::vector<uint8_t> response(65536);
    pipe::DWORD bytesRead = 0;
    if (!client_.ReadAll(response.data(),
                         static_cast<DWORD>(response.size()), bytesRead)) {
        return false;
    }

    return pipe::ReadSearchResponseStream(
        response.data(), bytesRead,
        [&](const pipe::SearchResult& r) {
            CliSearchResult sr;
            sr.Name = ToWString(r.Name);
            sr.Path = ToWString(r.Path);
            sr.IsDir = r.IsDir;
            sr.Drive = ToWString(r.Drive);
            sr.RankSortKey = r.RankSortKey;
            onResult(sr);
        });
}

} // namespace swiftlist::cli
