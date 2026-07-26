#pragma once

#include "ui/directwrite_text.h"
#include "ui/window.h"

#include <functional>
#include <string>

namespace swiftlist::ui {

class TextBlock {
public:
    TextBlock() = default;

    void SetText(const std::wstring& text);
    void SetStyle(const TextStyle& style);
    void SetBounds(const D2D1_RECT_F& rect);
    const D2D1_RECT_F& Bounds() const { return bounds_; }

    void Draw(D2DSurface& surface);

private:
    std::wstring text_;
    TextStyle style_;
    D2D1_RECT_F bounds_{};
};

class Button {
public:
    using ClickFn = std::function<void()>;

    Button() = default;

    void SetText(const std::wstring& text);
    void SetBounds(const D2D1_RECT_F& rect);
    void SetOnClick(ClickFn cb);
    const D2D1_RECT_F& Bounds() const { return bounds_; }

    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void Draw(D2DSurface& surface);

private:
    enum class State { Normal, Hover, Pressed, Disabled };

    std::wstring text_;
    D2D1_RECT_F bounds_{};
    ClickFn onClick_;
    State state_ = State::Normal;
    bool pressed_ = false;
};

} // namespace swiftlist::ui
