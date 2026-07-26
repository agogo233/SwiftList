#pragma once

#include "ui/direct2d_surface.h"

#include <string>

namespace swiftlist::ui {

enum class TextAlignment {
    Leading,
    Center,
    Trailing,
    Justified,
};

enum class ParagraphAlignment {
    Near,
    Center,
    Far,
};

struct TextStyle {
    std::wstring FontFamily = L"Segoe UI";
    float FontSize = 14.0f;
    uint32_t Color = 0xFFCCCCCC;
    DWRITE_FONT_WEIGHT Weight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STYLE Style = DWRITE_FONT_STYLE_NORMAL;
    TextAlignment HAlign = TextAlignment::Leading;
    ParagraphAlignment VAlign = ParagraphAlignment::Near;
    bool Wrap = true;
    bool Trim = true;
};

class TextRenderer {
public:
    explicit TextRenderer(D2DSurface& surface);

    void DrawText(const std::wstring& text, const D2D1_RECT_F& rect,
                   const TextStyle& style);

    D2D1_SIZE_F MeasureText(const std::wstring& text, float maxWidth,
                            const TextStyle& style);

private:
    ComPtr<IDWriteTextFormat> CreateFormat(const TextStyle& style);

    D2DSurface& surface_;
};

} // namespace swiftlist::ui
