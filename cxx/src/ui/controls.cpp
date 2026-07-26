#include "ui/controls.h"
#include "ui/theme.h"

#include <algorithm>

namespace swiftlist::ui {

void TextBlock::SetText(const std::wstring& text) {
    text_ = text;
}

void TextBlock::SetStyle(const TextStyle& style) {
    style_ = style;
}

void TextBlock::SetBounds(const D2D1_RECT_F& rect) {
    bounds_ = rect;
}

void TextBlock::Draw(D2DSurface& surface) {
    if (text_.empty()) return;
    TextRenderer renderer(surface);
    renderer.DrawText(text_, bounds_, style_);
}

void Button::SetText(const std::wstring& text) {
    text_ = text;
}

void Button::SetBounds(const D2D1_RECT_F& rect) {
    bounds_ = rect;
}

void Button::SetOnClick(ClickFn cb) {
    onClick_ = std::move(cb);
}

void Button::OnMouseMove(int x, int y) {
    if (state_ == State::Disabled) return;
    bool inside = (x >= bounds_.left && x <= bounds_.right &&
                   y >= bounds_.top && y <= bounds_.bottom);
    if (inside && state_ != State::Pressed) {
        state_ = State::Hover;
    } else if (!inside && state_ != State::Pressed) {
        state_ = State::Normal;
    }
}

void Button::OnLButtonDown(int x, int y) {
    if (state_ == State::Disabled) return;
    if (x >= bounds_.left && x <= bounds_.right &&
        y >= bounds_.top && y <= bounds_.bottom) {
        state_ = State::Pressed;
        pressed_ = true;
    }
}

void Button::OnLButtonUp(int x, int y) {
    if (state_ == State::Disabled) return;
    if (pressed_ && onClick_ && x >= bounds_.left && x <= bounds_.right &&
        y >= bounds_.top && y <= bounds_.bottom) {
        onClick_();
    }
    pressed_ = false;
    state_ = State::Normal;
}

void Button::Draw(D2DSurface& surface) {
    if (!surface.Context()) return;

    const auto& theme = ThemeManager::Instance().Current();
    uint32_t bgColor = theme.Colors.Surface;
    uint32_t fgColor = theme.Colors.Text;

    switch (state_) {
    case State::Hover:
        bgColor = theme.Colors.SurfaceHover; break;
    case State::Pressed:
        bgColor = theme.Colors.SurfaceActive; break;
    case State::Disabled:
        bgColor = theme.Colors.Surface;
        fgColor = theme.Colors.TextDisabled; break;
    default: break;
    }

    auto color = surface.ColorFromUint(bgColor);
    ComPtr<ID2D1SolidColorBrush> brush;
    surface.Context()->CreateSolidColorBrush(color, brush.GetAddressOf());

    float radius = theme.CornerRadius;
    D2D1_ROUNDED_RECT rrect = { bounds_, radius, radius };
    surface.Context()->FillRoundedRectangle(rrect, brush.Get());

    uint32_t borderColor = state_ == State::Hover ? theme.Colors.Primary
                                                  : theme.Colors.Border;
    auto bcolor = surface.ColorFromUint(borderColor);
    ComPtr<ID2D1SolidColorBrush> bbrush;
    surface.Context()->CreateSolidColorBrush(bcolor, bbrush.Get());
    surface.Context()->DrawRoundedRectangle(rrect, bbrush.Get(), 1.0f);

    if (!text_.empty()) {
        TextStyle tstyle;
        tstyle.FontSize = theme.FontSize;
        tstyle.FontFamily = theme.FontFamily;
        tstyle.Color = fgColor;
        tstyle.HAlign = TextAlignment::Center;
        tstyle.VAlign = ParagraphAlignment::Center;
        tstyle.Weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;

        TextRenderer renderer(surface);
        auto measure = renderer.MeasureText(
            text_, bounds_.right - bounds_.left, tstyle);
        D2D1_RECT_F textRect = {
            bounds_.left,
            bounds_.top + ((bounds_.bottom - bounds_.top) - measure.height) / 2.0f,
            bounds_.right,
            bounds_.bottom
        };
        renderer.DrawText(text_, textRect, tstyle);
    }
}

} // namespace swiftlist::ui
