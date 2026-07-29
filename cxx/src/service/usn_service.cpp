#include "service/usn_service.h"

#include <cstring>

namespace swiftlist::service {

WinService::WinService() = default;

WinService::~WinService() {
    Stop();
}

bool WinService::Run(const std::wstring& serviceName, ServiceRequestFn handler) {
    serviceName_ = serviceName;
    handler_ = std::move(handler);
    instance_ = this;

    SERVICE_TABLE_ENTRYW dispatchTable[] = {
        { const_cast<LPWSTR>(serviceName_.c_str()), ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcherW(dispatchTable)) {
        return false;
    }

    return true;
}

void WinService::Stop() {
    running_ = false;
    if (stopEvent_ != INVALID_HANDLE_VALUE) {
        SetEvent(stopEvent_);
    }
}

void WINAPI WinService::ServiceMain(DWORD /*argc*/, LPWSTR* /*argv*/) {
    if (!instance_) return;

    statusHandle_ = RegisterServiceCtrlHandlerExW(
        instance_->serviceName_.c_str(), ServiceCtrlHandler, nullptr);

    if (!statusHandle_) return;

    instance_->ReportStatus(SERVICE_START_PENDING);

    instance_->stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!instance_->stopEvent_) {
        instance_->ReportStatus(SERVICE_STOPPED);
        return;
    }

    instance_->running_ = true;
    instance_->ReportStatus(SERVICE_RUNNING);

    WaitForSingleObject(instance_->stopEvent_, INFINITE);

    instance_->ReportStatus(SERVICE_STOPPED);
}

DWORD WINAPI WinService::ServiceCtrlHandler(DWORD control, DWORD /*eventType*/,
                                              void* /*eventData*/,
                                              void* /*context*/) {
    if (!instance_) return;

    switch (control) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        instance_->ReportStatus(SERVICE_STOP_PENDING);
        if (handler_) handler_(control);
        instance_->Stop();
        break;

    case SERVICE_CONTROL_PAUSE:
    case SERVICE_CONTROL_CONTINUE:
        if (handler_) handler_(control);
        break;

    default:
        break;
    }
}

void WinService::ReportStatus(DWORD currentState, DWORD waitHint) {
    static DWORD checkPoint = 1;

    SERVICE_STATUS status = {};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = currentState;
    status.dwControlsAccepted = (currentState == SERVICE_RUNNING)
                                    ? (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN)
                                    : 0;
    status.dwWin32ExitCode = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwWaitHint = waitHint;
    status.dwCheckPoint = (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED)
                              ? 0
                              : checkPoint++;

    SetServiceStatus(statusHandle_, &status);
}

} // namespace swiftlist::service
