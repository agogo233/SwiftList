#pragma once

#include "app/pipe_client_wrapper.h"

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace swiftlist::app {

class SearchViewModel {
public:
    using ResultsFn = std::function<void(const std::vector<AppSearchResult>&)>;

    SearchViewModel();
    ~SearchViewModel();

    SearchViewModel(const SearchViewModel&) = delete;
    SearchViewModel& operator=(const SearchViewModel&) = delete;

    void SetPipeName(const std::wstring& name);
    void SetOnResults(ResultsFn cb);
    void SetDebounceMs(uint32_t ms);

    void OnQueryChanged(const std::wstring& query);
    void CancelPending();

private:
    void DebounceLoop();
    void ExecuteSearch(const std::wstring& query);

    std::wstring pipeName_;
    std::wstring currentQuery_;
    std::wstring pendingQuery_;
    uint32_t debounceMs_ = 50;
    ResultsFn onResults_;

    std::atomic<bool> running_{false};
    std::atomic<bool> queryDirty_{false};
    std::thread worker_;
    std::chrono::steady_clock::time_point lastChange_;
};

} // namespace swiftlist::app
