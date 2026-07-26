#pragma once

#include "app/pipe_client_wrapper.h"

#include <string>
#include <vector>

namespace swiftlist::app {

enum class ActionType {
    Open,
    OpenWith,
    CopyPath,
    CopyName,
    OpenContainingFolder,
    ShowProperties,
    RunAsAdmin,
    PinToQuickAccess,
};

struct ActionMenuItem {
    std::wstring Label;
    ActionType Type;
    bool Enabled = true;
};

class ActionMenuBuilder {
public:
    static std::vector<ActionMenuItem> BuildFileActions();
    static std::vector<ActionMenuItem> BuildDirectoryActions();
};

} // namespace swiftlist::app
