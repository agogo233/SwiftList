#include "cli/search_session.h"

namespace swiftlist::cli {

void SearchSession::SetPipeName(const std::wstring& name) {
    pipeName_ = name;
}

void SearchSession::SetMaxResults(int max) {
    maxResults_ = max;
}

void SearchSession::StartSearch(const std::wstring& query) {
    query_ = query;
    ClearResults();
    state_ = SessionState::Searching;

    if (pipeName_.empty()) {
        state_ = SessionState::Results;
        return;
    }

    AppSearchPipeClient client;
    if (!client.Connect(pipeName_)) {
        state_ = SessionState::Results;
        return;
    }

    client.Search(query, maxResults_, [this](const CliSearchResult& r) {
        results_.push_back(r);
    });

    client.Disconnect();
    state_ = SessionState::Results;
}

void SearchSession::CancelSearch() {
    state_ = SessionState::Results;
}

void SearchSession::ClearResults() {
    results_.clear();
}

} // namespace swiftlist::cli
