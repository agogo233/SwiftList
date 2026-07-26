#pragma once

#include "ui/window.h"

namespace swiftlist::ui {

class ScrollBar {
public:
    ScrollBar() = default;

    void SetBounds(const D2D1_RECT_F& rect);
    const D2D1_RECT_F& Bounds() const { return bounds_; }

    void SetRange(int minPos, int maxPos);
    void SetPageSize(int size);
    int Position() const { return position_; }
    void SetPosition(int pos);

    void OnLButtonDown(int x, int y);
    void OnMouseMove(int x, int y);
    void OnLButtonUp();
    void OnMouseWheel(int delta);

    void Draw(D2DSurface& surface);

private:
    D2D1_RECT_F ThumbRect() const;

    D2D1_RECT_F bounds_{};
    int minPos_ = 0;
    int maxPos_ = 100;
    int pageSize_ = 10;
    int position_ = 0;
    bool dragging_ = false;
    int dragStartY_ = 0;
    int dragStartPos_ = 0;
};

} // namespace swiftlist::ui
