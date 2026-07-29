#include "ui/directwrite_text.h"

namespace swiftlist::ui {

TextRenderer::TextRenderer(D2DSurface& surface) : surface_(surface) {}

ComPtr<IDWriteTextFormat> TextRenderer::CreateFormat(const TextStyle& style) {
    if (!surface_.WriteFactory()) return nullptr;

    ComPtr<IDWriteTextFormat> format;
    HRESULT hr = surface_.WriteFactory()->CreateTextFormat(
        style.FontFamily.c_str(), nullptr,
        style.Weight, style.Style, DWRITE_FONT_STRETCH_NORMAL,
        style.FontSize, L"en-us",
        format.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    switch (style.HAlign) {
    case TextAlignment::Center:
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); break;
    case TextAlignment::Trailing:
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); break;
    case TextAlignment::Justified:
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_JUSTIFIED); break;
    default:
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); break;
    }

    switch (style.VAlign) {
    case ParagraphAlignment::Center:
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); break;
    case ParagraphAlignment::Far:
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR); break;
    default:
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR); break;
    }

    format->SetWordWrapping(style.Wrap ? DWRITE_WORD_WRAPPING_WRAP
                                        : DWRITE_WORD_WRAPPING_NO_WRAP);

    return format;
}

void TextRenderer::DrawText(const std::wstring& text, const D2D1_RECT_F& rect,
                             const TextStyle& style) {
    if (!surface_.Context()) return;

    auto format = CreateFormat(style);
    if (!format) return;

    D2D1_COLOR_F color = surface_.ColorFromUint(style.Color);
    ComPtr<ID2D1SolidColorBrush> brush;
    HRESULT hr = surface_.Context()->CreateSolidColorBrush(color,
                                                              brush.GetAddressOf());
    if (FAILED(hr)) return;

    ComPtr<IDWriteTextLayout> layout;
    hr = surface_.WriteFactory()->CreateTextLayout(
        text.c_str(), static_cast<UINT32>(text.size()),
        format.Get(), rect.right - rect.left, rect.bottom - rect.top,
        layout.GetAddressOf());

    if (SUCCEEDED(hr) && style.Trim) {
        ComPtr<IDWriteInlineObject> trimming;
        DWRITE_TRIMMING trimmingOpt = {};
        trimmingOpt.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
        trimmingOpt.delimiter = L'\x2026';
        trimmingOpt.delimiterCount = 1;
        layout->SetTrimming(&trimmingOpt, trimming.Get());
    }

    if (SUCCEEDED(hr)) {
        surface_.Context()->DrawTextLayout(
            D2D1::Point2F(rect.left, rect.top),
            layout.Get(), brush.Get(),
            D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    }
}

D2D1_SIZE_F TextRenderer::MeasureText(const std::wstring& text, float maxWidth,
                                       const TextStyle& style) {
    D2D1_SIZE_F size = {0, 0};
    if (!surface_.WriteFactory()) return size;

    auto format = CreateFormat(style);
    if (!format) return size;

    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = surface_.WriteFactory()->CreateTextLayout(
        text.c_str(), static_cast<UINT32>(text.size()),
        format.Get(), maxWidth, 100000.0f,
        layout.GetAddressOf());
    if (FAILED(hr)) return size;

    DWRITE_TEXT_METRICS metrics = {};
    layout->GetMetrics(&metrics);
    size.width = metrics.widthIncludingTrailingWhitespace;
    size.height = metrics.height;
    return size;
}

} // namespace swiftlist::ui
