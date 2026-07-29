#include "ui/text_box.h"
#include "ui/theme.h"

#include <algorithm>

namespace swiftlist::ui {

void TextBox::SetBounds(const D2D1_RECT_F& rect) {
    bounds_ = rect;
}

void TextBox::SetText(const std::wstring& text) {
    text_ = text;
    caretPos_ = static_cast<int>(text_.size());
    selStart_ = caretPos_;
    selEnd_ = caretPos_;
    EnsureCaretVisible();
    if (onTextChanged_) onTextChanged_(text_);
}

void TextBox::SetPlaceholder(const std::wstring& text) {
    placeholder_ = text;
}

void TextBox::SetOnTextChanged(TextChangedFn cb) {
    onTextChanged_ = std::move(cb);
}

void TextBox::OnChar(wchar_t ch) {
    if (!focused_) return;

    if (ch == L'\b') {
        if (selStart_ != selEnd_) {
            text_.erase(selStart_, selEnd_ - selStart_);
            caretPos_ = selStart_;
        } else if (caretPos_ > 0) {
            text_.erase(caretPos_ - 1, 1);
            --caretPos_;
        }
        selStart_ = selEnd_ = caretPos_;
        EnsureCaretVisible();
        if (onTextChanged_) onTextChanged_(text_);
        return;
    }

    if (ch >= 32) {
        if (selStart_ != selEnd_) {
            text_.erase(selStart_, selEnd_ - selStart_);
            caretPos_ = selStart_;
        }
        text_.insert(caretPos_, 1, ch);
        ++caretPos_;
        selStart_ = selEnd_ = caretPos_;
        EnsureCaretVisible();
        if (onTextChanged_) onTextChanged_(text_);
    }
}

void TextBox::OnKeyDown(WPARAM key) {
    if (!focused_) return;

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    switch (key) {
    case VK_LEFT:
        if (caretPos_ > 0) --caretPos_;
        if (!shift) selStart_ = selEnd_ = caretPos_;
        else selEnd_ = caretPos_;
        break;
    case VK_RIGHT:
        if (caretPos_ < static_cast<int>(text_.size())) ++caretPos_;
        if (!shift) selStart_ = selEnd_ = caretPos_;
        else selEnd_ = caretPos_;
        break;
    case VK_HOME:
        caretPos_ = 0;
        if (!shift) selStart_ = selEnd_ = caretPos_;
        else selEnd_ = caretPos_;
        break;
    case VK_END:
        caretPos_ = static_cast<int>(text_.size());
        if (!shift) selStart_ = selEnd_ = caretPos_;
        else selEnd_ = caretPos_;
        break;
    case VK_DELETE:
        if (selStart_ != selEnd_) {
            text_.erase(selStart_, selEnd_ - selStart_);
            caretPos_ = selStart_;
        } else if (caretPos_ < static_cast<int>(text_.size())) {
            text_.erase(caretPos_, 1);
        }
        selStart_ = selEnd_ = caretPos_;
        if (onTextChanged_) onTextChanged_(text_);
        break;
    case VK_TAB:
        break;
    }
    EnsureCaretVisible();
}

void TextBox::OnLButtonDown(int x, int y) {
    if (x >= bounds_.left && x <= bounds_.right &&
        y >= bounds_.top && y <= bounds_.bottom) {
        focused_ = true;
        caretPos_ = HitTestText(x);
        selStart_ = selEnd_ = caretPos_;
        SetFocusCaret();
    } else {
        focused_ = false;
    }
}

void TextBox::SetFocus(bool focused) {
    focused_ = focused;
    if (focused) SetFocusCaret();
}

void TextBox::Draw(D2DSurface& surface) {
    if (!surface.Context()) return;

    const auto& theme = ThemeManager::Instance().Current();

    D2D1_COLOR_F bgColor = surface.ColorFromUint(theme.Colors.Background);
    ComPtr<ID2D1SolidColorBrush> bgBrush;
    surface.Context()->CreateSolidColorBrush(bgColor, bgBrush.Get());
    surface.Context()->FillRectangle(bounds_, bgBrush.Get());

    auto borderColor = focused_ ? theme.Colors.Primary : theme.Colors.Border;
    D2D1_COLOR_F bcolor = surface.ColorFromUint(borderColor);
    ComPtr<ID2D1SolidColorBrush> bbrush;
    surface.Context()->CreateSolidColorBrush(bcolor, bbrush.Get());
    surface.Context()->DrawRectangle(bounds_, bbrush.Get(), focused_ ? 2.0f : 1.0f);

    float pad = theme.Spacing.Sm;
    D2D1_RECT_F textRect = {
        bounds_.left + pad - scrollX_,
        bounds_.top + (bounds_.bottom - bounds_.top - theme.FontSize) / 2.0f,
        bounds_.right,
        bounds_.bottom
    };

    if (!text_.empty()) {
        TextStyle tstyle;
        tstyle.FontSize = theme.FontSize;
        tstyle.FontFamily = theme.FontFamily;
        tstyle.Color = theme.Colors.Text;

        TextRenderer renderer(surface);
        renderer.DrawText(text_, textRect, tstyle);
    } else if (!placeholder_.empty()) {
        TextStyle tstyle;
        tstyle.FontSize = theme.FontSize;
        tstyle.FontFamily = theme.FontFamily;
        tstyle.Color = theme.Colors.TextMuted;

        TextRenderer renderer(surface);
        renderer.DrawText(placeholder_, textRect, tstyle);
    }

    if (focused_ && caretVisible_) {
        TextStyle tstyle;
        tstyle.FontSize = theme.FontSize;
        tstyle.FontFamily = theme.FontFamily;
        TextRenderer renderer(surface);
        auto measure = renderer.MeasureText(
            text_.substr(0, caretPos_), bounds_.right - bounds_.left, tstyle);

        float caretX = textRect.left + measure.width;
        D2D1_COLOR_F caretColor = surface.ColorFromUint(theme.Colors.Primary);
        ComPtr<ID2D1SolidColorBrush> caretBrush;
        surface.Context()->CreateSolidColorBrush(caretColor, caretBrush.Get());
        surface.Context()->DrawLine(
            D2D1::Point2F(caretX, textRect.top),
            D2D1::Point2F(caretX, textRect.top + theme.FontSize + 2.0f),
            caretBrush.Get(), 2.0f);
    }
}

void TextBox::EnsureCaretVisible() {
    const auto& theme = ThemeManager::Instance().Current();
    float approxWidth = static_cast<float>(caretPos_) * (theme.FontSize * 0.6f);
    float viewWidth = (bounds_.right - bounds_.left) - 2.0f * theme.Spacing.Sm;
    if (approxWidth - scrollX_ > viewWidth) {
        scrollX_ = approxWidth - viewWidth + theme.Spacing.Sm;
    } else if (approxWidth < scrollX_) {
        scrollX_ = std::max(0.0f, approxWidth - theme.Spacing.Sm);
    }
}

int TextBox::HitTestText(int x) const {
    const auto& theme = ThemeManager::Instance().Current();
    float localX = x - bounds_.left + scrollX_ - theme.Spacing.Sm;
    if (localX <= 0) return 0;
    float avgCharWidth = theme.FontSize * 0.6f;
    int approxIdx = static_cast<int>(localX / avgCharWidth);
    return std::clamp(approxIdx, 0, static_cast<int>(text_.size()));
}

void TextBox::SetFocusCaret() {
}

} // namespace swiftlist::ui
