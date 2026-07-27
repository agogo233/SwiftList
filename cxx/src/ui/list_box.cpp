#include "ui/list_box.h"
#include "ui/theme.h"

#include <algorithm>

namespace swiftlist::ui {

void ListBox::SetBounds(const D2D1_RECT_F& rect) {
    bounds_ = rect;
}

void ListBox::SetItems(const std::vector<ListBoxItem>& items) {
    items_ = items;
    scrollOffset_ = 0;
    selectedIndex_ = -1;
}

void ListBox::SetItemHeight(float height) {
    itemHeight_ = height;
}

void ListBox::SetOnSelect(SelectFn cb) {
    onSelect_ = std::move(cb);
}

void ListBox::SetSelectedIndex(int index) {
    if (index >= -1 && index < static_cast<int>(items_.size())) {
        selectedIndex_ = index;
    }
}

void ListBox::OnMouseMove(int x, int y) {
    if (x >= bounds_.left && x <= bounds_.right &&
        y >= bounds_.top && y <= bounds_.bottom) {
        hoveredIndex_ = ItemAt(y);
    } else {
        hoveredIndex_ = -1;
    }
}

void ListBox::OnLButtonDown(int x, int y) {
    if (x < bounds_.left || x > bounds_.right ||
        y < bounds_.top || y > bounds_.bottom) {
        return;
    }

    int idx = ItemAt(y);
    if (idx >= 0 && idx < static_cast<int>(items_.size())) {
        selectedIndex_ = idx;
        if (onSelect_) onSelect_(idx);
    }
}

void ListBox::OnMouseWheel(int delta) {
    int deltaItems = (delta > 0) ? -3 : 3;
    ScrollTo(scrollOffset_ + deltaItems);
}

void ListBox::ScrollTo(int offset) {
    int maxOffset = std::max(0, static_cast<int>(items_.size()) -
                                 static_cast<int>((bounds_.bottom - bounds_.top) / itemHeight_));
    scrollOffset_ = std::clamp(offset, 0, maxOffset);
}

int ListBox::SelectedIndex() const {
    return selectedIndex_;
}

void ListBox::Draw(D2DSurface& surface) {
    if (!surface.Context()) return;

    const auto& theme = ThemeManager::Instance().Current();

    auto bgColor = surface.ColorFromUint(theme.Colors.Surface);
    ComPtr<ID2D1SolidColorBrush> bgBrush;
    surface.Context()->CreateSolidColorBrush(bgColor, bgBrush.Get());
    surface.Context()->FillRectangle(bounds_, bgBrush.Get());

    float visibleHeight = bounds_.bottom - bounds_.top;
    int visibleCount = static_cast<int>(visibleHeight / itemHeight_) + 1;
    int startIdx = scrollOffset_;
    int endIdx = std::min(static_cast<int>(items_.size()), startIdx + visibleCount);

    for (int i = startIdx; i < endIdx; ++i) {
        auto rect = ItemRect(i);

        uint32_t bg = 0;
        if (i == selectedIndex_) {
            bg = theme.Colors.Primary;
        } else if (i == hoveredIndex_) {
            bg = theme.Colors.SurfaceHover;
        }
        if (bg != 0) {
            auto color = surface.ColorFromUint(bg);
            ComPtr<ID2D1SolidColorBrush> brush;
            surface.Context()->CreateSolidColorBrush(color, brush.Get());
            surface.Context()->FillRectangle(rect, brush.Get());
        }

        TextStyle tstyle;
        tstyle.FontSize = theme.FontSize;
        tstyle.FontFamily = theme.FontFamily;
        tstyle.Color = (i == selectedIndex_) ? theme.Colors.OnPrimary
                                              : theme.Colors.Text;

        float pad = theme.Spacing.Sm;
        D2D1_RECT_F textRect = {
            rect.left + pad,
            rect.top + (itemHeight_ - theme.FontSize) / 2.0f,
            rect.right - pad,
            rect.bottom
        };

        TextRenderer renderer(surface);
        renderer.DrawText(items_[i].Text, textRect, tstyle);
    }
}

float ListBox::TotalContentHeight() const {
    return static_cast<float>(items_.size()) * itemHeight_;
}

int ListBox::ItemAt(float y) const {
    float localY = y - bounds_.top;
    int idx = static_cast<int>(localY / itemHeight_) + scrollOffset_;
    if (idx < 0 || idx >= static_cast<int>(items_.size())) return -1;
    return idx;
}

D2D1_RECT_F ListBox::ItemRect(int index) const {
    float y = bounds_.top + static_cast<float>(index - scrollOffset_) * itemHeight_;
    return { bounds_.left, y, bounds_.right, y + itemHeight_ };
}

void ListBox::EnsureVisible(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) return;

    float visibleHeight = bounds_.bottom - bounds_.top;
    int visibleCount = static_cast<int>(visibleHeight / itemHeight_);

    if (index < scrollOffset_) {
        ScrollTo(index);
    } else if (index >= scrollOffset_ + visibleCount) {
        ScrollTo(index - visibleCount + 1);
    }
}

} // namespace swiftlist::ui
