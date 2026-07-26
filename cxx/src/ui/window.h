#pragma once

#include "ui/direct2d_surface.h"

#include <functional>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace swiftlist::ui {

enum class HitTestResult {
    None,
    Client,
    Caption,
    CloseButton,
    MinimizeButton,
    MaximizeButton,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
};

struct WindowInsets {
    float left = 0, top = 0, right = 0, bottom = 0;
};

class Window {
public:
    Window();
    virtual ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(const std::wstring& title, int x, int y, int width, int height,
                DWORD style = WS_OVERLAPPEDWINDOW,
                DWORD exStyle = WS_EX_NOREDIRECTIONBITMAP);
    void Destroy();

    HWND Handle() const { return hwnd_; }
    D2DSurface& Surface() { return surface_; }
    bool IsVisible() const { return hwnd_ && IsWindowVisible(hwnd_); }

    void Show(int nCmdShow = SW_SHOW);
    void Hide();
    void Invalidate();
    void SetTitle(const std::wstring& title);

    void SetExtendsIntoClientArea(const WindowInsets& insets);
    void EnableBlurBehind(bool enable);

    static int MessagePump();

protected:
    virtual LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    virtual void OnPaint();
    virtual void OnSize(uint32_t width, uint32_t height);
    virtual void OnDpiChanged(WORD dpiX, WORD dpiY, const RECT* rect);
    virtual void OnMouseMove(int x, int y, WPARAM keys);
    virtual void OnLButtonDown(int x, int y, WPARAM keys);
    virtual void OnLButtonUp(int x, int y, WPARAM keys);
    virtual void OnKeyDown(WPARAM key);
    virtual void OnKeyUp(WPARAM key);
    virtual void OnChar(wchar_t ch);
    virtual void OnDestroy();

    virtual HitTestResult HitTest(int x, int y);

    int DpiScale(int value) const;
    void SetDpi(uint32_t dpi);

    HWND hwnd_ = nullptr;
    D2DSurface surface_;
    WindowInsets extendInsets_{};
    uint32_t dpi_ = 96;
    bool customFrame_ = false;

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam);
    static Window* FromHandle(HWND hwnd);
};

} // namespace swiftlist::ui
