#include "hook/inline_search_manager.h"

#include "pipe/hook_ipc_message.h"
#include "pipe/pipe_client.h"

namespace swiftlist::hook {

using pipe::HookEventType;

InlineSearchManager::InlineSearchManager() = default;

InlineSearchManager::~InlineSearchManager() = default;

void InlineSearchManager::SetAppName(const std::wstring& name) {
    appName_ = name;
    eventPipeName_.clear();
}

void InlineSearchManager::SetSessionId(uint32_t sessionId) {
    sessionId_ = sessionId;
    eventPipeName_.clear();
}

void InlineSearchManager::NotifyApp(uint32_t processId) {
    if (!SendEvent(static_cast<uint8_t>(HookEventType::DoubleCtrl), processId)) {
        fwprintf(stderr, L"[InlineSearch] Failed to notify app pid=%u\n", processId);
    }
}

void InlineSearchManager::NotifySearchVisible(bool visible) {
    SendEvent(static_cast<uint8_t>(HookEventType::SearchVisible), visible ? 1u : 0u);
}

void InlineSearchManager::NotifySelectionChanged(const std::wstring& path) {
    // Known limitation: sends a 32-bit hash of the path, not the path itself,
    // because the Hook IPC frame only has 4 bytes of data. The App can detect
    // that the directory changed but cannot reconstruct the exact path.
    // Phase B should upgrade to variable-length frames if path awareness is needed.
    size_t hash = std::hash<std::wstring>{}(path);
    SendEvent(static_cast<uint8_t>(HookEventType::ExplorerPathChanged),
              static_cast<uint32_t>(hash & 0xFFFFFFFF));
}

void InlineSearchManager::NotifySearchFinished(bool cancelled) {
    SendEvent(static_cast<uint8_t>(HookEventType::SearchFinished), cancelled ? 1u : 0u);
}

bool InlineSearchManager::SendEvent(uint8_t eventType, uint32_t data) {
    if (eventPipeName_.empty()) {
        eventPipeName_ = L"\\\\.\\pipe\\SwiftList_Hook_Events_" + appName_ +
                          L"_" + std::to_wstring(sessionId_);
    }

    pipe::PipeClient client;
    if (!client.Connect(eventPipeName_, 1000)) return false;

    std::vector<uint8_t> buf;
    pipe::WriteHookEvent(buf, static_cast<pipe::HookEventType>(eventType), data);
    return client.WriteAll(buf.data(), static_cast<DWORD>(buf.size()));
}

} // namespace swiftlist::hook
