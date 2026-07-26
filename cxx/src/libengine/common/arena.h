#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace swiftlist {

// Linear bump allocator for Fzf DP matrix and other short-lived scratch buffers.
// Fixed capacity, no individual deallocation, reset all at once.
class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t capacity);
    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    ArenaAllocator(ArenaAllocator&& other) noexcept;
    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

    // Allocate `size` bytes with given alignment (must be power of 2)
    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    template <typename T>
    [[nodiscard]] T* AllocateArray(size_t count) {
        return static_cast<T*>(Allocate(count * sizeof(T), alignof(T)));
    }

    void Reset() noexcept { used_ = 0; }

    [[nodiscard]] size_t Used() const noexcept { return used_; }
    [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }
    [[nodiscard]] size_t Remaining() const noexcept { return capacity_ - used_; }

private:
    static size_t AlignUp(size_t offset, size_t alignment) {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    std::unique_ptr<std::byte[]> buffer_;
    size_t capacity_;
    size_t used_;
};

}  // namespace swiftlist
