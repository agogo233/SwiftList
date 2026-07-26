#include "ui/theme.h"

namespace swiftlist::ui {

ThemeManager& ThemeManager::Instance() {
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager() {
    Theme dark;
    dark.Name = "Dark";
    current_ = dark;
    themes_["Dark"] = dark;

    Theme light;
    light.Name = "Light";
    light.Colors.Background = 0xFFF3F3F3;
    light.Colors.Surface = 0xFFFFFFFF;
    light.Colors.SurfaceHover = 0xFFE5E5E5;
    light.Colors.SurfaceActive = 0xFFD4D4D4;
    light.Colors.Primary = 0xFF0078D4;
    light.Colors.OnPrimary = 0xFFFFFFFF;
    light.Colors.Text = 0xFF1A1A1A;
    light.Colors.TextDisabled = 0xFFA0A0A0;
    light.Colors.TextMuted = 0xFF666666;
    light.Colors.Border = 0xFFD1D1D1;
    light.Colors.Accent = 0xFF68217A;
    light.Colors.Shadow = 0x20000000;
    themes_["Light"] = light;
}

void ThemeManager::RegisterTheme(const Theme& theme) {
    themes_[theme.Name] = theme;
}

bool ThemeManager::Activate(const std::string& name) {
    auto it = themes_.find(name);
    if (it == themes_.end()) return false;

    current_ = it->second;
    for (auto& cb : callbacks_) {
        cb(current_);
    }
    return true;
}

const Theme& ThemeManager::Current() const {
    return current_;
}

void ThemeManager::OnChanged(ChangeCallback cb) {
    callbacks_.push_back(std::move(cb));
}

} // namespace swiftlist::ui
