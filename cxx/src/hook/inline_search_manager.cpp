#include "hook/inline_search_manager.h"

namespace swiftlist::hook {

InlineSearchManager::InlineSearchManager() = default;

InlineSearchManager::~InlineSearchManager() = default;

void InlineSearchManager::SetAppName(const std::wstring& name) {
    appName_ = name;
}

void InlineSearchManager::SetSessionId(uint32_t sessionId) {
    sessionId_ = sessionId;
}

void InlineSearchManager::NotifyApp(uint32_t /*processId*/) {
}

void InlineSearchManager::NotifySearchVisible(bool /*visible*/) {
}

void InlineSearchManager::NotifySelectionChanged(const std::wstring& /*path*/) {
}

void InlineSearchManager::NotifySearchFinished(bool /*cancelled*/) {
}

std::wstring InlineSearchManager::EventPipeName() const {
    return L"\\\\.\\pipe\\SwiftList_Hook_Events_" + appName + L"_" +
           std::to_wstring(sessionId_);
}

std::wstring InlineSearchManager::CmdPipeName() const {
    return L"\\\\.\\pipe\\SwiftList_Hook_Cmds_" + appName + L"_" +
           std::to_wstring(sessionId_);
}

} // namespace swiftlist::hook
