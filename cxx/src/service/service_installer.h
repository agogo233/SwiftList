#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <string>

namespace swiftlist::service {

struct ServiceInstallConfig {
    std::wstring ServiceName;
    std::wstring DisplayName;
    std::wstring BinPath;
    std::wstring Description;
    DWORD StartType = SERVICE_AUTO_START;
    std::wstring SecurityDescriptor;
};

bool ServiceInstall(const ServiceInstallConfig& config);

bool ServiceUninstall(const std::wstring& serviceName);

bool ServiceStart(const std::wstring& serviceName);

bool ServiceStop(const std::wstring& serviceName);

bool IsServiceInstalled(const std::wstring& serviceName);

} // namespace swiftlist::service
