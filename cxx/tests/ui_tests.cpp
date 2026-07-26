#include "ui/theme.h"
#include "ui/animation.h"
#include "ui/list_box.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace swiftlist::ui;

// --- Theme tests ---

TEST(ThemeTest, DefaultDarkTheme) {
    auto& mgr = ThemeManager::Instance();
    const auto& theme = mgr.Current();
    EXPECT_EQ(theme.Name, "Dark");
    EXPECT_NE(theme.Colors.Background, 0);
}

TEST(ThemeTest, ActivateLightTheme) {
    auto& mgr = ThemeManager::Instance();
    EXPECT_TRUE(mgr.Activate("Light"));
    EXPECT_EQ(mgr.Current().Name, "Light");
    EXPECT_TRUE(mgr.Activate("Dark"));
}

TEST(ThemeTest, ActivateNonexistent) {
    auto& mgr = ThemeManager::Instance();
    EXPECT_FALSE(mgr.Activate("Nonexistent"));
}

TEST(ThemeTest, ChangeCallback) {
    auto& mgr = ThemeManager::Instance();
    bool called = false;
    mgr.OnChanged([&called](const Theme&) { called = true; });
    mgr.Activate("Light");
    EXPECT_TRUE(called);
    mgr.Activate("Dark");
}

// --- Animation tests ---

TEST(AnimationTest, EasingFunctions) {
    EXPECT_FLOAT_EQ(EaseValue(0.0f, Easing::Linear), 0.0f);
    EXPECT_FLOAT_EQ(EaseValue(1.0f, Easing::Linear), 1.0f);
    EXPECT_FLOAT_EQ(EaseValue(0.5f, Easing::Linear), 0.5f);
    EXPECT_FLOAT_EQ(EaseValue(0.0f, Easing::EaseOut), 0.0f);
    EXPECT_FLOAT_EQ(EaseValue(1.0f, Easing::EaseOut), 1.0f);
    EXPECT_LE(EaseValue(0.5f, Easing::EaseIn), 0.5f);
    EXPECT_GE(EaseValue(0.5f, Easing::EaseOut), 0.5f);
}

TEST(AnimationTest, AnimationLifecycle) {
    Animation anim;
    EXPECT_FALSE(anim.IsRunning());

    float lastValue = 0.0f;
    bool completed = false;
    anim.Start(0.0f, 100.0f, 100, Easing::Linear,
               [&](float v) { lastValue = v; },
               [&]() { completed = true; });

    EXPECT_TRUE(anim.IsRunning());

    for (int i = 0; i < 10; ++i) {
        anim.Tick();
    }

    EXPECT_GE(lastValue, 0.0f);
    EXPECT_LE(lastValue, 100.0f);
}

TEST(AnimationTest, AnimationCompletes) {
    Animation anim;
    bool completed = false;
    anim.Start(0.0f, 1.0f, 10, Easing::Linear,
               [](float) {}, [&]() { completed = true; });

    for (int i = 0; i < 20; ++i) {
        anim.Tick();
    }

    EXPECT_TRUE(completed);
    EXPECT_FALSE(anim.IsRunning());
}

// --- ListBox tests ---

TEST(ListBoxTest, SetItems) {
    ListBox lb;
    std::vector<ListBoxItem> items;
    for (int i = 0; i < 100; ++i) {
        items.push_back({L"Item " + std::to_wstring(i)});
    }
    lb.SetItems(items);
    EXPECT_EQ(lb.SelectedIndex(), -1);
}

TEST(ListBoxTest, SelectItem) {
    ListBox lb;
    std::vector<ListBoxItem> items;
    for (int i = 0; i < 10; ++i) {
        items.push_back({L"Item " + std::to_wstring(i)});
    }
    lb.SetItems(items);
    lb.SetBounds({0, 0, 200, 400});
    lb.SetItemHeight(40.0f);

    int selectedIdx = -1;
    lb.SetOnSelect([&](int idx) { selectedIdx = idx; });

    lb.OnLButtonDown(10, 50);
    EXPECT_GE(selectedIdx, 0);
    EXPECT_EQ(lb.SelectedIndex(), selectedIdx);
}

TEST(ListBoxTest, ScrollTo) {
    ListBox lb;
    std::vector<ListBoxItem> items;
    for (int i = 0; i < 1000; ++i) {
        items.push_back({L"Item " + std::to_wstring(i)});
    }
    lb.SetItems(items);
    lb.SetBounds({0, 0, 200, 400});
    lb.SetItemHeight(40.0f);

    lb.ScrollTo(50);
    EXPECT_GE(lb.Bounds().top, 0.0f);
}

TEST(ListBoxTest, EnsureVisible) {
    ListBox lb;
    std::vector<ListBoxItem> items;
    for (int i = 0; i < 100; ++i) {
        items.push_back({L"Item " + std::to_wstring(i)});
    }
    lb.SetItems(items);
    lb.SetBounds({0, 0, 200, 400});
    lb.SetItemHeight(40.0f);

    lb.EnsureVisible(50);
    EXPECT_GE(lb.Bounds().top, 0.0f);
}
