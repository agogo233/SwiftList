#include "hook/mouse_hook.h"

namespace swiftlist::hook {

MouseHook* MouseHook::instance_ = nullptr;

MouseHook::MouseHook() {
    instance_ = this;
}

MouseHook::~MouseHook() {
    Uninstall();
    instance_ = nullptr;
}

bool MouseHook::Install() {
    if (hook_) return true;

    hook_ = SetWindowsHookExW(WH_MOUSE_LL, LowLevelProc,
                               GetModuleHandleW(nullptr), 0);
    return hook_ != nullptr;
}

void MouseHook::Uninstall() {
    if (hook_) {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
}

void MouseHook::SetCallback(MouseFn cb) {
    callback_ = std::move(cb);
}

LRESULT CALLBACK MouseHook::LowLevelProc(int nCode, WPARAM wParam,
                                           LPARAM lParam) {
    if (instance_) {
        return instance_->HandleMouse(nCode, wParam, lParam);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT MouseHook::HandleMouse(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(hook_, nCode, wParam, lParam);
    }

    auto* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    MouseEvent event;
    event.X = mouse->pt.x;
    event.Y = mouse->pt.y;

    switch (wParam) {
    case WM_LBUTTONDOWN:
        event.LeftButton = true;
        break;
    case WM_LBUTTONDBLCLK:
        event.LeftButton = true;
        event.DoubleClick = true;
        break;
    case WM_RBUTTONDOWN:
        event.RightButton = true;
        break;
    case WM_MBUTTONDOWN:
        event.MiddleButton = true;
        break;
    }

    if (callback_) {
        callback_(event);
    }

    return CallNextHookEx(hook_, nCode, wParam, lParam);
}

} // namespace swiftlist::hook
