#include "hook/keyboard_hook.h"

namespace swiftlist::hook {

KeyboardHook* KeyboardHook::instance_ = nullptr;

KeyboardHook::KeyboardHook() {
    instance_ = this;
}

KeyboardHook::~KeyboardHook() {
    Uninstall();
    instance_ = nullptr;
}

bool KeyboardHook::Install() {
    if (hook_) return true;

    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelProc,
                               GetModuleHandleW(nullptr), 0);
    return hook_ != nullptr;
}

void KeyboardHook::Uninstall() {
    if (hook_) {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
}

void KeyboardHook::SetCallback(KeyFn cb) {
    callback_ = std::move(cb);
}

LRESULT CALLBACK KeyboardHook::LowLevelProc(int nCode, WPARAM wParam,
                                              LPARAM lParam) {
    if (instance_) {
        return instance_->HandleKey(nCode, wParam, lParam);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT KeyboardHook::HandleKey(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(hook_, nCode, wParam, lParam);
    }

    auto* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    bool keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    bool keyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    KeyEvent event;

    if (kbd->vkCode == VK_LCONTROL || kbd->vkCode == VK_RCONTROL ||
        kbd->vkCode == VK_CONTROL) {
        if (keyDown) {
            ctrlDown_ = true;
            if (IsDoubleCtrl()) {
                event.Type = KeyEventType::CtrlDoublePress;
            }
        } else if (keyUp) {
            ctrlDown_ = false;
        }
    } else if (keyDown) {
        switch (kbd->vkCode) {
        case VK_BACK: event.Type = KeyEventType::Backspace; break;
        case VK_ESCAPE: event.Type = KeyEventType::Escape; break;
        case VK_RETURN: event.Type = KeyEventType::Enter; break;
        case VK_UP: event.Type = KeyEventType::Up; break;
        case VK_DOWN: event.Type = KeyEventType::Down; break;
        case VK_LEFT: event.Type = KeyEventType::Left; break;
        case VK_RIGHT: event.Type = KeyEventType::Right; break;
        default:
            if (kbd->vkCode >= '0' && kbd->vkCode <= '9' && ctrlDown_) {
                event.Type = KeyEventType::CtrlNumber;
                event.Number = kbd->vkCode - '0';
            } else if (kbd->vkCode >= 32 && kbd->vkCode < 127) {
                event.Type = KeyEventType::Char;
                event.Char = static_cast<wchar_t>(kbd->vkCode);
            }
            break;
        }
    }

    if (event.Type != KeyEventType::None && callback_) {
        callback_(event);
    }

    return CallNextHookEx(hook_, nCode, wParam, lParam);
}

bool KeyboardHook::IsDoubleCtrl() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - lastCtrlTime_)
                       .count();

    if (elapsed < kDoubleCtrlMs) {
        ctrlCount_++;
    } else {
        ctrlCount_ = 1;
    }
    lastCtrlTime_ = now;

    return ctrlCount_ >= 2;
}

} // namespace swiftlist::hook
