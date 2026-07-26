#pragma once

#include "cli/search_session.h"
#include "cli/terminal.h"

namespace swiftlist::cli {

class Renderer {
public:
    explicit Renderer(Terminal& term);

    void DrawPrompt(const std::wstring& query);
    void DrawResults(const std::vector<CliSearchResult>& results,
                      int selectedIndex, int scrollOffset);
    void DrawStatusBar(const std::wstring& message);
    void ClearResults();

private:
    Terminal& term_;
    int resultsStartRow_ = 2;
    int resultsEndRow_ = 0;
};

} // namespace swiftlist::cli
