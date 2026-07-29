#pragma once

#include "ui/directwrite_text.h"
#include "ui/window.h"

#include <functional>
#include <string>

namespace swiftlist::ui {

class TextBox {
public:
    using TextChangedFn = std::function<void(const std::wstring&)>;

    TextBox() = default;

    void SetBounds(const D2D1_RECT_F& rect);
    const D2D1_RECT_F& Bounds() const { return bounds_; }

    const std::wstring& Text() const { return text_; }
    void SetText(const std::wstring& text);
    void SetPlaceholder(const std::wstring& text);
    void SetOnTextChanged(TextChangedFn cb);

    void OnChar(wchar_t ch);
    void OnKeyDown(WPARAM key);
    void OnLButtonDown(int x, int y);

    void SetFocus(bool focused);
    bool HasFocus() const { return focused_; }

    void Draw(D2DSurface& surface);

private:
    void EnsureCaretVisible();
    int HitTestText(int x) const;
    void SetFocusCaret();

    std::wstring text_;
    std::wstring placeholder_;
    D2D1_RECT_F bounds_{};
    TextChangedFn onTextChanged_;
    int caretPos_ = 0;
    int selStart_ = 0;
    int selEnd_ = 0;
    bool focused_ = false;
    float scrollX_ = 0.0f;
    uint64_t lastBlinkTick_ = 0;
    bool caretVisible_ = true;
};

} // namespace swiftlist::ui
