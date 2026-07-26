#include "cli/terminal.h"
#include "cli/app_search_pipe_client.h"
#include "cli/search_session.h"
#include "cli/renderer.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace swiftlist::cli;

int wmain(int argc, wchar_t* argv[]) {
    Terminal term;
    if (!term.Initialize()) {
        fwprintf(stderr, L"Failed to initialize terminal\n");
        return 1;
    }

    term.Clear();

    std::wstring pipeName = L"\\\\.\\pipe\\SwiftListPipe";
    if (argc > 1) {
        pipeName = argv[1];
    }

    SearchSession session;
    session.SetPipeName(pipeName);
    session.SetMaxResults(50);

    Renderer renderer(term);
    renderer.DrawStatusBar(L" SwiftList CLI Search - Type to search, ESC to quit ");

    std::wstring query;
    int selectedIndex = 0;
    int scrollOffset = 0;
    bool quit = false;

    while (!quit) {
        renderer.DrawPrompt(query);
        renderer.DrawResults(session.Results(), selectedIndex, scrollOffset);

        wchar_t ch = term.ReadKey();

        if (ch == 27) {
            quit = true;
        } else if (ch == L'\b') {
            if (!query.empty()) {
                query.pop_back();
                session.StartSearch(query);
                selectedIndex = 0;
                scrollOffset = 0;
            }
        } else if (ch == L'\r' || ch == L'\n') {
            if (!session.Results().empty() && selectedIndex >= 0 &&
                selectedIndex < static_cast<int>(session.Results().size())) {
                term.WriteLine(session.Results()[selectedIndex].Path);
                quit = true;
            }
        } else if (ch == L'\t' || ch == 0) {
            ch = term.ReadKey();
            if (ch == 38 && selectedIndex > 0) {
                --selectedIndex;
                if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
            } else if (ch == 40 &&
                       selectedIndex < static_cast<int>(session.Results().size()) - 1) {
                ++selectedIndex;
            }
        } else if (ch >= 32) {
            query.push_back(ch);
            session.StartSearch(query);
            selectedIndex = 0;
            scrollOffset = 0;
        }
    }

    term.Clear();
    term.SetCursorPosition(0, 0);
    return 0;
}
