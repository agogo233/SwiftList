#include "ui/icon_cache.h"

#include <shellapi.h>

namespace swiftlist::ui {

IconCache& IconCache::Instance() {
    static IconCache inst;
    return inst;
}

void IconCache::SetMaxSize(size_t maxEntries) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxSize_ = maxEntries;
}

void IconCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

ComPtr<ID2D1Bitmap> IconCache::GetIcon(const std::wstring& path,
                                         D2DSurface& surface) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(path);
    if (it != cache_.end()) {
        it->second.LastAccess = ++tick_;
        return it->second.Bitmap;
    }

    if (cache_.size() >= maxSize_) {
        std::wstring oldestKey;
        uint64_t oldestTick = UINT64_MAX;
        for (auto& [k, v] : cache_) {
            if (v.LastAccess < oldestTick) {
                oldestTick = v.LastAccess;
                oldestKey = k;
            }
        }
        if (!oldestKey.empty()) cache_.erase(oldestKey);
    }

    HICON icon = ExtractIcon(path);
    if (!icon) return nullptr;

    ComPtr<ID2D1Bitmap> bitmap = surface.BitmapFromIcon(icon);
    if (bitmap) {
        IconCacheEntry entry;
        entry.Bitmap = bitmap;
        entry.LastAccess = ++tick_;
        cache_[path] = std::move(entry);
    }

    DestroyIcon(icon);
    return bitmap;
}

#ifdef ExtractIcon
#undef ExtractIcon
#endif

HICON IconCache::ExtractIcon(const std::wstring& path) {
    SHFILEINFOW sfi = {};
    DWORD_PTR result = SHGetFileInfoW(
        path.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    return result ? sfi.hIcon : nullptr;
}

void ImageBox::SetBounds(const D2D1_RECT_F& rect) {
    bounds_ = rect;
}

void ImageBox::SetIconPath(const std::wstring& path) {
    iconPath_ = path;
    bitmap_ = nullptr;
}

void ImageBox::SetBitmap(ComPtr<ID2D1Bitmap> bitmap) {
    bitmap_ = std::move(bitmap);
}

void ImageBox::Draw(D2DSurface& surface) {
    if (!surface.Context()) return;

    if (!bitmap_ && !iconPath_.empty()) {
        bitmap_ = IconCache::Instance().GetIcon(iconPath_, surface);
    }

    if (!bitmap_) return;

    D2D1_SIZE_F size = bitmap_->GetSize();
    float scaleX = (bounds_.right - bounds_.left) / size.width;
    float scaleY = (bounds_.bottom - bounds_.top) / size.height;
    float scale = std::min(scaleX, scaleY);

    float w = size.width * scale;
    float h = size.height * scale;
    float x = bounds_.left + ((bounds_.right - bounds_.left) - w) / 2.0f;
    float y = bounds_.top + ((bounds_.bottom - bounds_.top) - h) / 2.0f;

    surface.Context()->DrawBitmap(
        bitmap_.Get(),
        D2D1::RectF(x, y, x + w, y + h),
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

} // namespace swiftlist::ui
