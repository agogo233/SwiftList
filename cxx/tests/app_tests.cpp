#include "app/action_menu_builder.h"

#include <gtest/gtest.h>

using namespace swiftlist::app;

TEST(ActionMenuBuilderTest, FileActionsNotEmpty) {
    auto actions = ActionMenuBuilder::BuildFileActions();
    EXPECT_FALSE(actions.empty());

    bool hasOpen = false;
    bool hasCopyPath = false;
    for (const auto& a : actions) {
        if (a.Type == ActionType::Open) hasOpen = true;
        if (a.Type == ActionType::CopyPath) hasCopyPath = true;
    }
    EXPECT_TRUE(hasOpen);
    EXPECT_TRUE(hasCopyPath);
}

TEST(ActionMenuBuilderTest, DirectoryActionsNotEmpty) {
    auto actions = ActionMenuBuilder::BuildDirectoryActions();
    EXPECT_FALSE(actions.empty());

    bool hasPin = false;
    for (const auto& a : actions) {
        if (a.Type == ActionType::PinToQuickAccess) hasPin = true;
    }
    EXPECT_TRUE(hasPin);
}

TEST(ActionMenuBuilderTest, FileActionsNoDisabled) {
    auto actions = ActionMenuBuilder::BuildFileActions();
    for (const auto& a : actions) {
        EXPECT_TRUE(a.Enabled);
    }
}
