#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <string>

namespace swiftlist::hook {

class InlineSearchManager {
public:
    InlineSearchManager();
    ~InlineSearchManager();

    InlineSearchManager(const InlineSearchManager&) = delete;
    InlineSearchManager& operator=(const InlineSearchManager&) = delete;

    void SetAppName(const std::wstring& name);
    void SetSessionId(uint32_t sessionId);

    void NotifyApp(uint32_t processId);
    void NotifySearchVisible(bool visible);
    void NotifySelectionChanged(const std::wstring& path);
    void NotifySearchFinished(bool cancelled);

private:
    bool SendEvent(uint8_t eventType, uint32_t data = 0);

    std::wstring appName_;
    uint32_t sessionId_ = 0;
    std::wstring eventPipeName_;
};

} // namespace swiftlist::hook
