#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <functional>

namespace swiftlist::hook {

struct MouseEvent {
    int X = 0;
    int Y = 0;
    bool LeftButton = false;
    bool RightButton = false;
    bool MiddleButton = false;
    bool DoubleClick = false;
};

class MouseHook {
public:
    using MouseFn = std::function<void(const MouseEvent&)>;

    MouseHook();
    ~MouseHook();

    MouseHook(const MouseHook&) = delete;
    MouseHook& operator=(const MouseHook&) = delete;

    bool Install();
    void Uninstall();
    void SetCallback(MouseFn cb);

    static MouseHook* Instance() { return instance_; }

private:
    static LRESULT CALLBACK LowLevelProc(int nCode, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMouse(int nCode, WPARAM wParam, LPARAM lParam);

    static inline MouseHook* instance_;

    HHOOK hook_ = nullptr;
    MouseFn callback_;
};

} // namespace swiftlist::hook
