#include "common/arena.h"

#include <cassert>
#include <cstdlib>
#include <utility>

namespace swiftlist {

ArenaAllocator::ArenaAllocator(size_t capacity)
    : buffer_(std::make_unique<std::byte[]>(capacity))
    , capacity_(capacity)
    , used_(0) {}

ArenaAllocator::~ArenaAllocator() = default;

ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
    : buffer_(std::move(other.buffer_))
    , capacity_(other.capacity_)
    , used_(other.used_) {
    other.capacity_ = 0;
    other.used_ = 0;
}

ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& other) noexcept {
    if (this != &other) {
        buffer_ = std::move(other.buffer_);
        capacity_ = other.capacity_;
        used_ = other.used_;
        other.capacity_ = 0;
        other.used_ = 0;
    }
    return *this;
}

void* ArenaAllocator::Allocate(size_t size, size_t alignment) {
    assert((alignment & (alignment - 1)) == 0 && "alignment must be power of 2");

    const size_t aligned = AlignUp(used_, alignment);
    if (aligned + size > capacity_) {
        return nullptr;  // Out of memory (caller should fall back to heap)
    }

    void* ptr = buffer_.get() + aligned;
    used_ = aligned + size;
    return ptr;
}

}  // namespace swiftlist
