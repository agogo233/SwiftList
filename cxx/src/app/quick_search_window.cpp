#include "app/quick_search_window.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace swiftlist::app {

QuickSearchWindow::QuickSearchWindow() {
    viewModel_.SetOnResults([this](const std::vector<AppSearchResult>& results) {
        SetResults(results);
    });
}

void QuickSearchWindow::ShowCentered() {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int w = DpiScale(600);
    int h = DpiScale(400);
    int x = (screenW - w) / 2;
    int y = (screenH - h) / 4;

    Create(L"", x, y, w, h,
           WS_POPUP, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED);

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
    std::vector<ui::ListBoxItem> items;
    items.reserve(results.size());
    for (const auto& r : results) {
        items.push_back({r.Name, r.Path});
    }
    resultsList_.SetItems(items);
    Invalidate();
}

LRESULT QuickSearchWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
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
