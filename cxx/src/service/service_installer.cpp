#include "service/service_installer.h"

#include <sddl.h>

namespace swiftlist::service {

static SC_HANDLE OpenSCM(DWORD access) {
    return OpenSCManagerW(nullptr, nullptr, access);
}

bool ServiceInstall(const ServiceInstallConfig& config) {
    auto scm = OpenSCM(SC_MANAGER_CREATE_SERVICE);
    if (!scm) return false;

    auto svc = CreateServiceW(
        scm,
        config.ServiceName.c_str(),
        config.DisplayName.empty() ? config.ServiceName.c_str() : config.DisplayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        config.StartType,
        SERVICE_ERROR_NORMAL,
        config.BinPath.c_str(),
        nullptr, nullptr, nullptr,
        nullptr, nullptr);

    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    if (!config.Description.empty()) {
        SERVICE_DESCRIPTIONW desc = {};
        desc.lpDescription = const_cast<LPWSTR>(config.Description.c_str());
        ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);
    }

    if (!config.SecurityDescriptor.empty()) {
        PSECURITY_DESCRIPTOR sd = nullptr;
        ULONG sdLen = 0;
        if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
                config.SecurityDescriptor.c_str(), SDDL_REVISION_1, &sd, &sdLen)) {
            ChangeServiceConfig2W(svc, SERVICE_CONFIG_LAUNCH_PROTECTED, nullptr);
            LocalFree(sd);
        }
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return true;
}

bool ServiceUninstall(const std::wstring& serviceName) {
    auto scm = OpenSCM(SC_MANAGER_CONNECT);
    if (!scm) return false;

    auto svc = OpenServiceW(scm, serviceName.c_str(), DELETE | SERVICE_STOP);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS status = {};
    ControlService(svc, SERVICE_CONTROL_STOP, &status);

    bool result = DeleteService(svc) != FALSE;

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return result;
}

bool ServiceStart(const std::wstring& serviceName) {
    auto scm = OpenSCM(SC_MANAGER_CONNECT);
    if (!scm) return false;

    auto svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_START);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    bool result = StartServiceW(svc, 0, nullptr) != FALSE;

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return result;
}

bool ServiceStop(const std::wstring& serviceName) {
    auto scm = OpenSCM(SC_MANAGER_CONNECT);
    if (!scm) return false;

    auto svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_STOP);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS status = {};
    bool result = ControlService(svc, SERVICE_CONTROL_STOP, &status) != FALSE;

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return result;
}

bool IsServiceInstalled(const std::wstring& serviceName) {
    auto scm = OpenSCM(SC_MANAGER_CONNECT);
    if (!scm) return false;

    auto svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_QUERY_STATUS);
    bool found = (svc != nullptr);

    if (svc) CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return found;
}

} // namespace swiftlist::service
