#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <functional>

namespace swiftlist::hook {

enum class KeyEventType {
    None,
    CtrlDoublePress,
    Backspace,
    Escape,
    Enter,
    Up,
    Down,
    Left,
    Right,
    Char,
    CtrlNumber,
};

struct KeyEvent {
    KeyEventType Type = KeyEventType::None;
    wchar_t Char = 0;
    int Number = 0;
};

class KeyboardHook {
public:
    using KeyFn = std::function<void(const KeyEvent&)>;

    KeyboardHook();
    ~KeyboardHook();

    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

    bool Install();
    void Uninstall();
    void SetCallback(KeyFn cb);

    static KeyboardHook* Instance() { return instance_; }

private:
    static LRESULT CALLBACK LowLevelProc(int nCode, WPARAM wParam, LPARAM lParam);
    LRESULT HandleKey(int nCode, WPARAM wParam, LPARAM lParam);
    bool IsDoubleCtrl();

    static inline KeyboardHook* instance_;

    HHOOK hook_ = nullptr;
    KeyFn callback_;

    std::chrono::steady_clock::time_point lastCtrlTime_;
    bool ctrlDown_ = false;
    int ctrlCount_ = 0;
    static constexpr int kDoubleCtrlMs = 400;
};

} // namespace swiftlist::hook
