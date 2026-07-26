#include "app/pipe_client_wrapper.h"

#include <WinNls.h>

namespace swiftlist::app {

PipeClientWrapper::~PipeClientWrapper() {
    Disconnect();
}

std::wstring PipeClientWrapper::ToWString(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                   static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                         static_cast<int>(s.size()), result.data(), len);
    return result;
}

bool PipeClientWrapper::Connect(const std::wstring& pipeName) {
    std::lock_guard<std::mutex> lock(mutex_);
    return client_.Connect(pipeName);
}

void PipeClientWrapper::Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (client_.IsConnected()) {
        client_.Disconnect();
    }
}

bool PipeClientWrapper::IsConnected() const {
    return client_.IsConnected();
}

bool PipeClientWrapper::SendSearch(const std::wstring& query, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastResults_.clear();

    if (!client_.IsConnected()) return false;

    std::string queryUtf8;
    if (!query.empty()) {
        int len = WideCharToMultiByte(CP_UTF8, 0, query.c_str(),
                                       static_cast<int>(query.size()),
                                       nullptr, 0, nullptr, nullptr);
        queryUtf8.resize(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, query.c_str(),
                             static_cast<int>(query.size()),
                             queryUtf8.data(), len, nullptr, nullptr);
    }

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
    return client_.ReadAll(response.data(),
                           static_cast<DWORD>(response.size()), bytesRead) &&
           bytesRead > 0 &&
           pipe::ReadSearchResponseStream(
               response.data(), bytesRead,
               [this](const pipe::SearchResult& r) {
                   AppSearchResult sr;
                   sr.Name = ToWString(r.Name);
                   sr.Path = ToWString(r.Path);
                   sr.IsDir = r.IsDir;
                   sr.Drive = ToWString(r.Drive);
                   sr.RankSortKey = r.RankSortKey;
                   sr.Metadata = r.Metadata;
                   lastResults_.push_back(std::move(sr));
               });
}

} // namespace swiftlist::app
