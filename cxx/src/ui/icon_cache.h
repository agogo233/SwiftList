#pragma once

#include "ui/direct2d_surface.h"

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace swiftlist::ui {

struct IconCacheEntry {
    ComPtr<ID2D1Bitmap> Bitmap;
    uint64_t LastAccess;
};

class IconCache {
public:
    static IconCache& Instance();

    ComPtr<ID2D1Bitmap> GetIcon(const std::wstring& path, D2DSurface& surface);
    void SetMaxSize(size_t maxEntries);
    void Clear();

private:
    IconCache() = default;

    std::mutex mutex_;
    std::unordered_map<std::wstring, IconCacheEntry> cache_;
    size_t maxSize_ = 256;
    uint64_t tick_ = 0;

    HICON ExtractIcon(const std::wstring& path);
};

class ImageBox {
public:
    ImageBox() = default;

    void SetBounds(const D2D1_RECT_F& rect);
    const D2D1_RECT_F& Bounds() const { return bounds_; }

    void SetIconPath(const std::wstring& path);
    void SetBitmap(ComPtr<ID2D1Bitmap> bitmap);

    void Draw(D2DSurface& surface);

private:
    D2D1_RECT_F bounds_{};
    std::wstring iconPath_;
    ComPtr<ID2D1Bitmap> bitmap_;
};

} // namespace swiftlist::ui
