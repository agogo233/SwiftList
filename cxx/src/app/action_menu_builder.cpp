#include "app/action_menu_builder.h"

namespace swiftlist::app {

std::vector<ActionMenuItem> ActionMenuBuilder::BuildFileActions() {
    return {
        {L"Open", ActionType::Open},
        {L"Open with...", ActionType::OpenWith},
        {L"Open containing folder", ActionType::OpenContainingFolder},
        {L"Copy path", ActionType::CopyPath},
        {L"Copy name", ActionType::CopyName},
        {L"Show properties", ActionType::ShowProperties},
        {L"Run as administrator", ActionType::RunAsAdmin},
    };
}

std::vector<ActionMenuItem> ActionMenuBuilder::BuildDirectoryActions() {
    return {
        {L"Open", ActionType::Open},
        {L"Open in new window", ActionType::OpenWith},
        {L"Open containing folder", ActionType::OpenContainingFolder},
        {L"Copy path", ActionType::CopyPath},
        {L"Pin to Quick Access", ActionType::PinToQuickAccess},
        {L"Show properties", ActionType::ShowProperties},
    };
}

} // namespace swiftlist::app
