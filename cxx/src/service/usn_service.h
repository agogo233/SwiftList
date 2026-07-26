#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <functional>
#include <string>

namespace swiftlist::service {

using ServiceRequestFn = std::function<void(DWORD control)>;

class WinService {
public:
    WinService();
    ~WinService();

    WinService(const WinService&) = delete;
    WinService& operator=(const WinService&) = delete;

    bool Run(const std::wstring& serviceName, ServiceRequestFn handler);
    void Stop();

    static void Install(const std::wstring& exePath);
    static void Uninstall(const std::wstring& serviceName);

private:
    static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
    static void WINAPI ServiceCtrlHandler(DWORD control);
    void ReportStatus(DWORD currentState, DWORD waitHint = 0);

    static inline WinService* instance_;
    static inline SERVICE_STATUS_HANDLE statusHandle_;
    static inline ServiceRequestFn handler_;

    std::wstring serviceName_;
    HANDLE stopEvent_ = INVALID_HANDLE_VALUE;
    std::atomic<bool> running_{false};
};

} // namespace swiftlist::service
