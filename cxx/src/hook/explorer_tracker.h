#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <functional>
#include <string>

namespace swiftlist::hook {

struct ExplorerInfo {
    HWND Handle = nullptr;
    std::wstring Path;
    std::wstring Caption;
    bool IsDesktop = false;
};

class ExplorerTracker {
public:
    using ChangeFn = std::function<void(const ExplorerInfo&)>;

    ExplorerTracker();
    ~ExplorerTracker();

    ExplorerTracker(const ExplorerTracker&) = delete;
    ExplorerTracker& operator=(const ExplorerTracker&) = delete;

    bool Start();
    void Stop();
    void SetCallback(ChangeFn cb);

    const ExplorerInfo& Current() const { return current_; }

    static ExplorerTracker* Instance() { return instance_; }

private:
    static void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event,
                                       HWND hwnd, LONG idObject,
                                       LONG idChild, DWORD eventThread,
                                       DWORD eventTime);
    void HandleForeground(HWND hwnd);
    static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);

    static inline ExplorerTracker* instance_;

    HWINEVENTHOOK hook_ = nullptr;
    ChangeFn callback_;
    ExplorerInfo current_;
};

} // namespace swiftlist::hook
