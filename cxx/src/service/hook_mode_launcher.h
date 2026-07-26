#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

namespace swiftlist::service {

struct HookLaunchResult {
    bool success = false;
    DWORD processId = 0;
    HANDLE processHandle = nullptr;
};

HookLaunchResult LaunchHookProcess(const std::wstring& exePath,
                                    const std::wstring& pipeName,
                                    bool requestElevation);

void TerminateHookProcess(HANDLE processHandle, DWORD processId);

} // namespace swiftlist::service
