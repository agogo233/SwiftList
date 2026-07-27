#pragma once

#include "ui/directwrite_text.h"
#include "ui/window.h"

#include <functional>
#include <string>
#include <vector>

namespace swiftlist::ui {

struct ListBoxItem {
    std::wstring Text;
    std::wstring Subtitle;
    void* UserData = nullptr;
};

class ListBox {
public:
    using SelectFn = std::function<void(int index)>;

    ListBox() = default;

    void SetBounds(const D2D1_RECT_F& rect);
    const D2D1_RECT_F& Bounds() const { return bounds_; }

    void SetItems(const std::vector<ListBoxItem>& items);
    void SetItemHeight(float height);
    void SetOnSelect(SelectFn cb);
    void SetSelectedIndex(int index);

    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnMouseWheel(int delta);
    void ScrollTo(int offset);
    int SelectedIndex() const { return selectedIndex_; }

    void Draw(D2DSurface& surface);

    void EnsureVisible(int index);

private:
    float TotalContentHeight() const;
    int ItemAt(float y) const;
    D2D1_RECT_F ItemRect(int index) const;

    D2D1_RECT_F bounds_{};
    std::vector<ListBoxItem> items_;
    float itemHeight_ = 40.0f;
    int scrollOffset_ = 0;
    int selectedIndex_ = -1;
    int hoveredIndex_ = -1;
    SelectFn onSelect_;
};

} // namespace swiftlist::ui
