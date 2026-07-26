#include "ui/window.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) static_cast<int>(static_cast<short>(LOWORD(lp)))
#define GET_Y_LPARAM(lp) static_cast<int>(static_cast<short>(HIWORD(lp)))
#endif

namespace swiftlist::ui {

Window::Window() = default;

Window::~Window() {
    Destroy();
}

bool Window::Create(const std::wstring& title, int x, int y, int width, int height,
                     DWORD style, DWORD exStyle) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = StaticWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SwiftListWindowClass";

    static bool registered = false;
    if (!registered) {
        RegisterClassExW(&wc);
        registered = true;
    }

    hwnd_ = CreateWindowExW(
        exStyle, wc.lpszClassName, title.c_str(),
        style, x, y, width, height,
        nullptr, nullptr, wc.hInstance, this);

    if (!hwnd_) return false;

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    dpi_ = GetDpiForWindow(hwnd_);

    return true;
}

void Window::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void Window::Show(int nCmdShow) {
    if (hwnd_) ShowWindow(hwnd_, nCmdShow);
}

void Window::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void Window::Invalidate() {
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::SetTitle(const std::wstring& title) {
    if (hwnd_) SetWindowTextW(hwnd_, title.c_str());
}

void Window::SetExtendsIntoClientArea(const WindowInsets& insets) {
    extendInsets_ = insets;
    customFrame_ = true;
}

void Window::EnableBlurBehind(bool enable) {
    if (!hwnd_) return;

    DBE_BUFFER_SIZE size = {};
    size.dwSize = sizeof(size);

    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = enable;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(hwnd_, &bb);
    DeleteObject(bb.hRgnBlur);
}

int Window::MessagePump() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT Window::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        OnPaint();
        ValidateRect(hwnd_, nullptr);
        return 0;
    }
    case WM_SIZE: {
        uint32_t w = LOWORD(lParam);
        uint32_t h = HIWORD(lParam);
        OnSize(w, h);
        return 0;
    }
    case WM_DPICHANGED: {
        WORD dpiX = LOWORD(wParam);
        WORD dpiY = HIWORD(wParam);
        auto* rect = reinterpret_cast<const RECT*>(lParam);
        OnDpiChanged(dpiX, dpiY, rect);
        return 0;
    }
    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;
    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;
    case WM_LBUTTONUP:
        OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;
    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;
    case WM_KEYUP:
        OnKeyUp(wParam);
        return 0;
    case WM_CHAR:
        OnChar(static_cast<wchar_t>(wParam));
        return 0;
    case WM_DESTROY:
        OnDestroy();
        return 0;
    case WM_NCCALCSIZE:
        if (customFrame_ && wParam == TRUE) {
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        if (customFrame_) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd_, &pt);
            auto ht = HitTest(pt.x, pt.y);
            switch (ht) {
            case HitTestResult::Caption: return HTCAPTION;
            case HitTestResult::CloseButton: return HTCLOSE;
            case HitTestResult::MinimizeButton: return HTMINBUTTON;
            case HitTestResult::MaximizeButton: return HTMAXBUTTON;
            case HitTestResult::TopLeft: return HTTOPLEFT;
            case HitTestResult::Top: return HTTOP;
            case HitTestResult::TopRight: return HTTOPRIGHT;
            case HitTestResult::Right: return HTRIGHT;
            case HitTestResult::BottomRight: return HTBOTTOMRIGHT;
            case HitTestResult::Bottom: return HTBOTTOM;
            case HitTestResult::BottomLeft: return HTBOTTOMLEFT;
            case HitTestResult::Left: return HTLEFT;
            default: return HTCLIENT;
            }
        }
        break;
    }
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void Window::OnPaint() {
    if (!surface_.Context()) {
        if (!surface_.CreateDeviceResources(hwnd_)) return;
    }

    PAINTSTRUCT ps = {};
    BeginPaint(hwnd_, &ps);

    surface_.BeginDraw();
    surface_.Context()->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    HRESULT hr = surface_.EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        surface_.DiscardDeviceResources();
    }

    EndPaint(hwnd_, &ps);
}

void Window::OnSize(uint32_t width, uint32_t height) {
    if (surface_.Context() && width > 0 && height > 0) {
        surface_.Resize(width, height);
    }
}

void Window::OnDpiChanged(WORD dpiX, WORD /*dpiY*/, const RECT* rect) {
    SetDpi(dpiX);
    if (rect) {
        SetWindowPos(hwnd_, nullptr, rect->left, rect->top,
                     rect->right - rect->left, rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    Invalidate();
}

void Window::OnMouseMove(int /*x*/, int /*y*/, WPARAM /*keys*/) {}
void Window::OnLButtonDown(int /*x*/, int /*y*/, WPARAM /*keys*/) {}
void Window::OnLButtonUp(int /*x*/, int /*y*/, WPARAM /*keys*/) {}
void Window::OnKeyDown(WPARAM /*key*/) {}
void Window::OnKeyUp(WPARAM /*key*/) {}
void Window::OnChar(wchar_t /*ch*/) {}
void Window::OnDestroy() { hwnd_ = nullptr; }

HitTestResult Window::HitTest(int /*x*/, int /*y*/) {
    return HitTestResult::Client;
}

int Window::DpiScale(int value) const {
    return static_cast<int>((static_cast<long long>(value) * dpi_) / 96);
}

void Window::SetDpi(uint32_t dpi) {
    if (dpi > 0) dpi_ = dpi;
}

LRESULT CALLBACK Window::StaticWndProc(HWND hwnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam) {
    auto* self = FromHandle(hwnd);

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self) {
        return self->WndProc(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

Window* Window::FromHandle(HWND hwnd) {
    return reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

} // namespace swiftlist::ui
