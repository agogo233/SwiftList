#include "app/app.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace swiftlist::app {

Application& Application::Instance() {
    static Application inst;
    return inst;
}

bool Application::Initialize() {
    if (initialized_) return true;

    if (!EnsureSingleInstance()) {
        return false;
    }

    quickSearch_.viewModel_.SetPipeName(L"\\\\.\\pipe\\SwiftListPipe");
    initialized_ = true;
    return true;
}

void Application::Shutdown() {
    if (singleInstanceMutex_) {
        ReleaseMutex(singleInstanceMutex_);
        CloseHandle(singleInstanceMutex_);
        singleInstanceMutex_ = nullptr;
    }
    initialized_ = false;
}

int Application::Run() {
    return ui::Window::MessagePump();
}

void Application::ToggleQuickSearch() {
    if (quickSearch_.IsVisible()) {
        quickSearch_.Dismiss();
    } else {
        quickSearch_.ShowCentered();
    }
}

void Application::ShowSettings() {
}

bool Application::EnsureSingleInstance() {
    singleInstanceMutex_ = CreateMutexW(nullptr, TRUE, L"SwiftListAppSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (singleInstanceMutex_) {
            CloseHandle(singleInstanceMutex_);
            singleInstanceMutex_ = nullptr;
        }
        return false;
    }
    return true;
}

} // namespace swiftlist::app
