#pragma once

#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace swiftlist::ui {

using Microsoft::WRL::ComPtr;

struct D2DPixelSize {
    uint32_t width;
    uint32_t height;
};

class D2DSurface {
public:
    D2DSurface();
    ~D2DSurface();

    D2DSurface(const D2DSurface&) = delete;
    D2DSurface& operator=(const D2DSurface&) = delete;

    bool CreateDeviceResources(HWND hwnd);
    void DiscardDeviceResources();
    void Resize(uint32_t width, uint32_t height);
    void SetDpi(float dpiX, float dpiY);

    void BeginDraw();
    HRESULT EndDraw();

    ID2D1DeviceContext* Context() const { return context_.Get(); }
    ID2D1Factory1* Factory() const { return factory_.Get(); }
    IDWriteFactory* WriteFactory() const { return dwriteFactory_.Get(); }
    IWICImagingFactory* WICFactory() const { return wicFactory_.Get(); }

    D2D1::ColorF ColorFromUint(uint32_t rgba) const;

    ComPtr<ID2D1Bitmap> BitmapFromIcon(HICON icon);

private:
    bool CreateDeviceIndependentResources();

    ComPtr<ID2D1Factory1> factory_;
    ComPtr<ID2D1Device> device_;
    ComPtr<ID2D1DeviceContext> context_;
    ComPtr<IDXGISwapChain1> swapChain_;
    ComPtr<IDXGISurface> backBuffer_;
    ComPtr<ID2D1Bitmap1> targetBitmap_;

    ComPtr<IDWriteFactory> dwriteFactory_;
    ComPtr<IWICImagingFactory> wicFactory_;

    float dpiX_ = 96.0f;
    float dpiY_ = 96.0f;
};

} // namespace swiftlist::ui
