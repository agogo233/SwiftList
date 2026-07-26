#include "cli/renderer.h"

namespace swiftlist::cli {

Renderer::Renderer(Terminal& term) : term_(term) {
    resultsEndRow_ = term.Height() - 2;
}

void Renderer::DrawPrompt(const std::wstring& query) {
    term_.SetCursorPosition(0, 0);
    term_.SetTextAttribute(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    term_.Write(L"Search: ");
    term_.ResetTextAttribute();
    term_.Write(query);
    term_.Write(L"_");
}

void Renderer::DrawResults(const std::vector<CliSearchResult>& results,
                            int selectedIndex, int scrollOffset) {
    int row = resultsStartRow_;
    int maxRow = resultsEndRow_;

    int endIdx = std::min(static_cast<int>(results.size()),
                           scrollOffset + (maxRow - resultsStartRow_));

    for (int i = scrollOffset; i < endIdx && row < maxRow; ++i) {
        term_.SetCursorPosition(0, row);

        if (i == selectedIndex) {
            term_.SetTextAttribute(BACKGROUND_BLUE | BACKGROUND_INTENSITY);
        }

        const auto& r = results[i];
        std::wstring line = r.Path;
        if (line.size() > static_cast<size_t>(term_.Width() - 1)) {
            line = line.substr(0, term_.Width() - 4) + L"...";
        }

        term_.Write(line);

        if (i == selectedIndex) {
            term_.ResetTextAttribute();
        }

        ++row;
    }

    while (row < maxRow) {
        term_.SetCursorPosition(0, row);
        std::wstring spaces(term_.Width(), L' ');
        term_.Write(spaces);
        ++row;
    }
}

void Renderer::DrawStatusBar(const std::wstring& message) {
    term_.SetCursorPosition(0, term_.Height() - 1);
    term_.SetTextAttribute(BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN |
                            FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    std::wstring padded = message;
    if (padded.size() < static_cast<size_t>(term_.Width())) {
        padded += std::wstring(term_.Width() - padded.size(), L' ');
    }
    term_.Write(padded);
    term_.ResetTextAttribute();
}

void Renderer::ClearResults() {
    int maxRow = resultsEndRow_;
    for (int row = resultsStartRow_; row < maxRow; ++row) {
        term_.SetCursorPosition(0, row);
        std::wstring spaces(term_.Width(), L' ');
        term_.Write(spaces);
    }
}

} // namespace swiftlist::cli
