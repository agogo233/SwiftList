#pragma once

#include "ui/window.h"
#include "ui/text_box.h"
#include "ui/list_box.h"
#include "app/search_view_model.h"

namespace swiftlist::app {

class QuickSearchWindow : public ui::Window {
public:
    QuickSearchWindow();
    ~QuickSearchWindow() override = default;

    void ShowCentered();
    void Dismiss();
    void SetResults(const std::vector<AppSearchResult>& results);

protected:
    LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
    void OnPaint() override;
    void OnKeyDown(WPARAM key) override;
    void OnChar(wchar_t ch) override;
    void OnDestroy() override;

private:
    void LayoutControls();
    void ExecuteSelected();
    void NavigateSelection(int delta);

    ui::TextBox searchBox_;
    ui::ListBox resultsList_;
    SearchViewModel viewModel_;
    std::vector<AppSearchResult> lastResults_;
    bool visible_ = false;
};

} // namespace swiftlist::app
