#include "service/hook_mode_launcher.h"

#include <shellapi.h>

namespace swiftlist::service {

HookLaunchResult LaunchHookProcess(const std::wstring& exePath,
                                    const std::wstring& pipeName,
                                    bool requestElevation) {
    HookLaunchResult result;

    if (requestElevation) {
        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"runas";
        sei.lpFile = exePath.c_str();
        sei.lpParameters = pipeName.c_str();
        sei.nShow = SW_SHOWNORMAL;

        if (ShellExecuteExW(&sei)) {
            result.success = true;
            result.processHandle = sei.hProcess;
            if (sei.hProcess) {
                result.processId = GetProcessId(sei.hProcess);
            }
        }
        return result;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::wstring cmdLine = L"\"" + exePath + L"\" " + pipeName;

    BOOL ok = CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr, nullptr, FALSE,
        0, nullptr, nullptr,
        &si, &pi);

    if (ok) {
        result.success = true;
        result.processId = pi.dwProcessId;
        result.processHandle = pi.hProcess;
        CloseHandle(pi.hThread);
    }

    return result;
}

void TerminateHookProcess(HANDLE processHandle, DWORD /*processId*/) {
    if (processHandle && processHandle != INVALID_HANDLE_VALUE) {
        TerminateProcess(processHandle, 1);
        WaitForSingleObject(processHandle, 5000);
        CloseHandle(processHandle);
    }
}

} // namespace swiftlist::service
