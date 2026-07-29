#pragma once

#include "app/pipe_client_wrapper.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace swiftlist::app {

class SearchViewModel {
public:
    SearchViewModel();
    ~SearchViewModel();

    SearchViewModel(const SearchViewModel&) = delete;
    SearchViewModel& operator=(const SearchViewModel&) = delete;

    void SetPipeName(const std::wstring& name);
    void SetDebounceMs(uint32_t ms);
    void SetWindow(HWND hwnd);

    void OnQueryChanged(const std::wstring& query);
    void CancelPending();

    static constexpr UINT kResultsMessage = WM_APP + 1;

private:
    void DebounceLoop();
    void ExecuteSearch(const std::wstring& query);

    std::wstring pipeName_;
    std::wstring currentQuery_;
    std::wstring pendingQuery_;
    uint32_t debounceMs_ = 50;
    HWND hwnd_ = nullptr;

    std::atomic<bool> running_{false};
    std::atomic<bool> queryDirty_{false};
    std::thread worker_;
    std::chrono::steady_clock::time_point lastChange_;
};

} // namespace swiftlist::app
