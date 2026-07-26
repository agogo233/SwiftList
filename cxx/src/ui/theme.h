#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace swiftlist::ui {

struct ThemeColors {
    uint32_t Background = 0xFF1E1E1E;
    uint32_t Surface = 0xFF252526;
    uint32_t SurfaceHover = 0xFF2A2D2E;
    uint32_t SurfaceActive = 0xFF37373D;
    uint32_t Primary = 0xFF0078D4;
    uint32_t PrimaryHover = 0xFF1BA1E2;
    uint32_t OnPrimary = 0xFFFFFFFF;
    uint32_t Text = 0xFFCCCCCC;
    uint32_t TextDisabled = 0xFF656565;
    uint32_t TextMuted = 0xFF808080;
    uint32_t Border = 0xFF3F3F46;
    uint32_t Accent = 0xFF68217A;
    uint32_t Error = 0xFFE81123;
    uint32_t Success = 0xFF107C10;
    uint32_t Shadow = 0x40000000;
};

struct ThemeSpacing {
    float Xs = 4.0f;
    float Sm = 8.0f;
    float Md = 12.0f;
    float Lg = 16.0f;
    float Xl = 24.0f;
};

struct ThemeMotion {
    uint32_t FastMs = 100;
    uint32_t NormalMs = 200;
    uint32_t SlowMs = 350;
};

struct ThemeElevation {
    float Level0 = 0.0f;
    float Level1 = 2.0f;
    float Level2 = 4.0f;
    float Level3 = 8.0f;
    float Level4 = 16.0f;
};

struct Theme {
    std::string Name;
    ThemeColors Colors;
    ThemeSpacing Spacing;
    ThemeMotion Motion;
    ThemeElevation Elevation;
    float CornerRadius = 4.0f;
    float FontSize = 14.0f;
    std::wstring FontFamily = L"Segoe UI";
};

class ThemeManager {
public:
    static ThemeManager& Instance();

    void RegisterTheme(const Theme& theme);
    bool Activate(const std::string& name);
    const Theme& Current() const;

    using ChangeCallback = std::function<void(const Theme&)>;
    void OnChanged(ChangeCallback cb);

private:
    ThemeManager();

    std::unordered_map<std::string, Theme> themes_;
    Theme current_;
    std::vector<ChangeCallback> callbacks_;
};

} // namespace swiftlist::ui
