#include "app/search_view_model.h"

#include <sstream>

namespace swiftlist::app {

SearchViewModel::SearchViewModel() = default;

SearchViewModel::~SearchViewModel() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

void SearchViewModel::SetPipeName(const std::wstring& name) {
    pipeName_ = name;
}

void SearchViewModel::SetOnResults(ResultsFn cb) {
    onResults_ = std::move(cb);
}

void SearchViewModel::SetDebounceMs(uint32_t ms) {
    debounceMs_ = ms;
}

void SearchViewModel::OnQueryChanged(const std::wstring& query) {
    std::ostringstream nullGuard;
    pendingQuery_ = query;
    lastChange_ = std::chrono::steady_clock::now();
    queryDirty_ = true;

    if (!running_) {
        running_ = true;
        if (worker_.joinable()) worker_.join();
        worker_ = std::thread(&SearchViewModel::DebounceLoop, this);
    }
}

void SearchViewModel::CancelPending() {
    queryDirty_ = false;
    pendingQuery_.clear();
}

void SearchViewModel::DebounceLoop() {
    while (running_) {
        if (!queryDirty_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - lastChange_)
                           .count();

        if (elapsed < static_cast<int64_t>(debounceMs_)) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(debounceMs_ - static_cast<uint32_t>(elapsed)));
            continue;
        }

        auto query = pendingQuery_;
        queryDirty_ = false;

        ExecuteSearch(query);
    }
}

void SearchViewModel::ExecuteSearch(const std::wstring& query) {
    PipeClientWrapper client;
    if (!client.Connect(pipeName_)) {
        if (onResults_) onResults_({});
        return;
    }

    client.SendSearch(query, 50);

    if (onResults_) {
        onResults_(client.LastResults());
    }
}

} // namespace swiftlist::app
