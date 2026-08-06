#include "app/quick_search_window.h"
#include "ui/theme.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>

namespace swiftlist::app {

QuickSearchWindow::QuickSearchWindow() = default;

void QuickSearchWindow::ShowCentered() {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int w = DpiScale(600);
    int h = DpiScale(400);
    int x = (screenW - w) / 2;
    int y = (screenH - h) / 4;

    Create(L"", x, y, w, h,
           WS_POPUP, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED);

    viewModel_.SetWindow(Handle());

    SetLayeredWindowAttributes(hwnd_, 0, 240, LWA_ALPHA);

    searchBox_.SetFocus(true);
    visible_ = true;
    Show();
}

void QuickSearchWindow::Dismiss() {
    visible_ = false;
    Hide();
    searchBox_.SetText(L"");
    searchBox_.SetFocus(false);
}

void QuickSearchWindow::SetResults(const std::vector<AppSearchResult>& results) {
    lastResults_ = results;
    std::vector<ui::ListBoxItem> items;
    items.reserve(results.size());
    for (const auto& r : results) {
        items.push_back({r.Name, r.Path});
    }
    resultsList_.SetItems(items);
    Invalidate();
}

LRESULT QuickSearchWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == SearchViewModel::kResultsMessage) {
        auto* results = reinterpret_cast<std::vector<AppSearchResult>*>(lParam);
        SetResults(*results);
        delete results;
        return 0;
    }
    switch (msg) {
    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;
    case WM_CHAR:
        OnChar(static_cast<wchar_t>(wParam));
        return 0;
    }
    return Window::WndProc(msg, wParam, lParam);
}

void QuickSearchWindow::OnPaint() {
    if (!Surface().Context()) {
        if (!Surface().CreateDeviceResources(hwnd_)) return;
    }

    auto* ctx = Surface().Context();
    const auto& theme = ui::ThemeManager::Instance().Current();

    ctx->BeginDraw();

    auto bgColor = Surface().ColorFromUint(theme.Colors.Background);
    ctx->Clear(bgColor);

    float pad = theme.Spacing.Md;
    float boxH = theme.FontSize + theme.Spacing.Md * 2;

    searchBox_.SetBounds({pad, pad, static_cast<float>(DpiScale(600)) - pad * 2, pad + boxH});
    resultsList_.SetBounds({pad, pad + boxH + theme.Spacing.Sm,
                            static_cast<float>(DpiScale(600)) - pad * 2,
                            static_cast<float>(DpiScale(400)) - pad * 2});

    searchBox_.Draw(Surface());
    resultsList_.Draw(Surface());

    ctx->EndDraw();
}

void QuickSearchWindow::OnKeyDown(WPARAM key) {
    switch (key) {
    case VK_ESCAPE:
        Dismiss();
        break;
    case VK_UP:
        NavigateSelection(-1);
        break;
    case VK_DOWN:
        NavigateSelection(1);
        break;
    case VK_RETURN:
        ExecuteSelected();
        break;
    default:
        searchBox_.OnKeyDown(key);
        break;
    }
}

void QuickSearchWindow::OnChar(wchar_t ch) {
    searchBox_.OnChar(ch);
    std::wstring query = searchBox_.Text();
    viewModel_.OnQueryChanged(query);
    Invalidate();
}

void QuickSearchWindow::OnDestroy() {
    viewModel_.CancelPending();
    Window::OnDestroy();
}

void QuickSearchWindow::LayoutControls() {}

void QuickSearchWindow::ExecuteSelected() {
    int idx = resultsList_.SelectedIndex();
    if (idx < 0) return;
    if (idx >= static_cast<int>(lastResults_.size())) return;

    const std::wstring& path = lastResults_[idx].Path;
    if (path.empty()) return;

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = path.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        fwprintf(stderr, L"[QuickSearch] Failed to open: %s (error %lu)\n",
                path.c_str(), GetLastError());
    }
}

void QuickSearchWindow::NavigateSelection(int delta) {
    int current = resultsList_.SelectedIndex();
    int next = current + delta;
    if (next >= 0) {
        resultsList_.SetSelectedIndex(next);
        resultsList_.EnsureVisible(next);
        Invalidate();
    }
}

} // namespace swiftlist::app
