#pragma once

#include "cli/app_search_pipe_client.h"

#include <string>
#include <vector>

namespace swiftlist::cli {

enum class SessionState {
    Idle,
    Searching,
    Results,
    Chosen,
};

class SearchSession {
public:
    SearchSession() = default;

    void SetPipeName(const std::wstring& name);
    void SetMaxResults(int max);

    void StartSearch(const std::wstring& query);
    void CancelSearch();

    const std::vector<CliSearchResult>& Results() const { return results_; }
    SessionState State() const { return state_; }
    const std::wstring& Query() const { return query_; }

    void ClearResults();

private:
    std::wstring pipeName_;
    std::wstring query_;
    std::vector<CliSearchResult> results_;
    SessionState state_ = SessionState::Idle;
    int maxResults_ = 50;
};

} // namespace swiftlist::cli
