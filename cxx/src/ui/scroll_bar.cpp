#include "ui/scroll_bar.h"
#include "ui/theme.h"

#include <algorithm>

namespace swiftlist::ui {

void ScrollBar::SetBounds(const D2D1_RECT_F& rect) {
    bounds_ = rect;
}

void ScrollBar::SetRange(int minPos, int maxPos) {
    minPos_ = minPos;
    maxPos_ = maxPos;
    SetPosition(position_);
}

void ScrollBar::SetPageSize(int size) {
    pageSize_ = size;
}

void ScrollBar::SetPosition(int pos) {
    int maxScroll = std::max(0, maxPos_ - minPos_ - pageSize_ + 1);
    position_ = std::clamp(pos, 0, maxScroll);
}

void ScrollBar::OnLButtonDown(int x, int y) {
    auto thumb = ThumbRect();
    if (x >= thumb.left && x <= thumb.right &&
        y >= thumb.top && y <= thumb.bottom) {
        dragging_ = true;
        dragStartY_ = y;
        dragStartPos_ = position_;
    } else if (y >= bounds_.top && y <= bounds_.bottom) {
        if (y < thumb.top) {
            SetPosition(position_ - pageSize_);
        } else if (y > thumb.bottom) {
            SetPosition(position_ + pageSize_);
        }
    }
}

void ScrollBar::OnMouseMove(int x, int y) {
    if (!dragging_) return;
    float trackHeight = bounds_.bottom - bounds_.top;
    int maxScroll = std::max(1, maxPos_ - minPos_ - pageSize_ + 1);
    int newPos = dragStartPos_ + static_cast<int>((y - dragStartY_) * maxScroll / trackHeight);
    SetPosition(newPos);
}

void ScrollBar::OnLButtonUp() {
    dragging_ = false;
}

void ScrollBar::OnMouseWheel(int delta) {
    SetPosition(position_ + ((delta > 0) ? -3 : 3));
}

void ScrollBar::Draw(D2DSurface& surface) {
    if (!surface.Context()) return;

    const auto& theme = ThemeManager::Instance().Current();

    D2D1_COLOR_F bgColor = surface.ColorFromUint(theme.Colors.Background);
    ComPtr<ID2D1SolidColorBrush> bgBrush;
    surface.Context()->CreateSolidColorBrush(bgColor, bgBrush.Get());
    surface.Context()->FillRectangle(bounds_, bgBrush.Get());

    auto thumb = ThumbRect();
    D2D1_COLOR_F thumbColor = surface.ColorFromUint(theme.Colors.Border);
    ComPtr<ID2D1SolidColorBrush> thumbBrush;
    surface.Context()->CreateSolidColorBrush(thumbColor, thumbBrush.Get());
    D2D1_ROUNDED_RECT rrect = { thumb, 3.0f, 3.0f };
    surface.Context()->FillRoundedRectangle(rrect, thumbBrush.Get());
}

D2D1_RECT_F ScrollBar::ThumbRect() const {
    int maxScroll = std::max(1, maxPos_ - minPos_ - pageSize_ + 1);
    float trackHeight = (bounds_.bottom - bounds_.top) - 4.0f;
    float thumbHeight = std::max(20.0f, trackHeight * static_cast<float>(pageSize_) /
                                             static_cast<float>(maxPos_ - minPos_ + 1));
    float thumbPos = (trackHeight - thumbHeight) * static_cast<float>(position_) /
                     static_cast<float>(maxScroll);
    return {
        bounds_.left + 2.0f,
        bounds_.top + 2.0f + thumbPos,
        bounds_.right - 2.0f,
        bounds_.top + 2.0f + thumbPos + thumbHeight
    };
}

} // namespace swiftlist::ui
