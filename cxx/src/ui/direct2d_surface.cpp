#include "ui/direct2d_surface.h"

#include <stdexcept>

namespace swiftlist::ui {

D2DSurface::D2DSurface() = default;

D2DSurface::~D2DSurface() {
    DiscardDeviceResources();
}

bool D2DSurface::CreateDeviceIndependentResources() {
    if (factory_) return true;

    D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(factory_.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IWICImagingFactory),
        reinterpret_cast<void**>(wicFactory_.GetAddressOf()));
    if (FAILED(hr)) return false;

    return true;
}

void D2DSurface::SetDpi(float dpiX, float dpiY) {
    dpiX_ = dpiX;
    dpiY_ = dpiY;
}

bool D2DSurface::CreateDeviceResources(HWND hwnd) {
    if (!CreateDeviceIndependentResources()) return false;

    if (context_) return true;

    RECT rc = {};
    GetClientRect(hwnd, &rc);
    D2DPixelSize size = {
        static_cast<uint32_t>(rc.right - rc.left),
        static_cast<uint32_t>(rc.bottom - rc.top)
    };

    D2D1_DEVICE_CONTEXT_OPTIONS deviceOptions = D2D1_DEVICE_CONTEXT_OPTIONS_NONE;
#ifdef _DEBUG
    deviceOptions = D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS;
#endif

    HRESULT hr = factory_->CreateDevice(
        DXGI_TYPE_IID_ARGS_DEFAULT,
        device_.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = device_->CreateDeviceContext(deviceOptions, context_.GetAddressOf());
    if (FAILED(hr)) return false;

    if (!hwnd) {
        return true;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = size.width;
    swapChainDesc.Height = size.height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    ComPtr<IDXGIDevice1> dxgiDevice;
    hr = device_.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) return false;

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = adapter->GetParent(__uuidof(IDXGIFactory2),
                            reinterpret_cast<void**>(dxgiFactory.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = dxgiFactory->CreateSwapChainForHwnd(
        device_.Get(), hwnd, &swapChainDesc,
        nullptr, nullptr, swapChain_.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = dxgiDevice->SetMaximumFrameLatency(1);
    if (FAILED(hr)) return false;

    hr = swapChain_->GetBuffer(0, __uuidof(IDXGISurface),
                                reinterpret_cast<void**>(backBuffer_.GetAddressOf()));
    if (FAILED(hr)) return false;

    D2D1_BITMAP_PROPERTIES1 bitmapProps = {};
    bitmapProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bitmapProps.pixelFormat.alphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    bitmapProps.dpiX = dpiX_;
    bitmapProps.dpiY = dpiY_;
    bitmapProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    hr = context_->CreateBitmapFromDxgiSurface(
        backBuffer_.Get(), bitmapProps, targetBitmap_.GetAddressOf());
    if (FAILED(hr)) return false;

    context_->SetTarget(targetBitmap_.Get());

    return true;
}

void D2DSurface::DiscardDeviceResources() {
    targetBitmap_.Reset();
    backBuffer_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
}

void D2DSurface::Resize(uint32_t width, uint32_t height) {
    if (!context_ || !swapChain_) return;

    context_->SetTarget(nullptr);
    targetBitmap_.Reset();
    backBuffer_.Reset();

    HRESULT hr = swapChain_->ResizeBuffers(0, width, height,
                                           DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return;

    hr = swapChain_->GetBuffer(0, __uuidof(IDXGISurface),
                                reinterpret_cast<void**>(backBuffer_.GetAddressOf()));
    if (FAILED(hr)) return;

    D2D1_BITMAP_PROPERTIES1 bitmapProps = {};
    bitmapProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bitmapProps.pixelFormat.alphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    bitmapProps.dpiX = dpiX_;
    bitmapProps.dpiY = dpiY_;
    bitmapProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    hr = context_->CreateBitmapFromDxgiSurface(
        backBuffer_.Get(), bitmapProps, targetBitmap_.GetAddressOf());
    if (FAILED(hr)) return;

    context_->SetTarget(targetBitmap_.Get());
}

void D2DSurface::BeginDraw() {
    if (context_) context_->BeginDraw();
}

HRESULT D2DSurface::EndDraw() {
    if (!context_) return E_FAIL;

    HRESULT hr = context_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        return hr;
    }

    if (swapChain_) {
        DXGI_PRESENT_PARAMETERS params = {};
        hr = swapChain_->Present1(1, 0, &params);
    }

    return hr;
}

D2D1_COLOR_F D2DSurface::ColorFromUint(uint32_t rgba) const {
    float r = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
    float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>(rgba & 0xFF) / 255.0f;
    float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
    return D2D1_COLOR_F{ r, g, b, a };
}

ComPtr<ID2D1Bitmap> D2DSurface::BitmapFromIcon(HICON icon) {
    if (!wicFactory_ || !context_) return nullptr;

    ComPtr<IWICBitmap> wicBitmap;
    HRESULT hr = wicFactory_->CreateBitmapFromHICON(icon,
                                                     wicBitmap.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    ComPtr<IWICFormatConverter> converter;
    hr = wicFactory_->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    hr = converter->Initialize(
        wicBitmap.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return nullptr;

    ComPtr<ID2D1Bitmap> bitmap;
    hr = context_->CreateBitmapFromWicBitmap(converter.Get(), nullptr,
                                              bitmap.GetAddressOf());
    return bitmap;
}

} // namespace swiftlist::ui
