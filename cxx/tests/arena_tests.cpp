#include <common/arena.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

using namespace swiftlist;

TEST(ArenaAllocator, DefaultState) {
    ArenaAllocator arena(1024);
    EXPECT_EQ(arena.Used(), 0u);
    EXPECT_EQ(arena.Capacity(), 1024u);
    EXPECT_EQ(arena.Remaining(), 1024u);
}

TEST(ArenaAllocator, AllocateIncreasesUsed) {
    ArenaAllocator arena(1024);
    void* ptr = arena.Allocate(64);
    EXPECT_NE(ptr, nullptr);
    EXPECT_GE(arena.Used(), 64u);
}

TEST(ArenaAllocator, AllocateArray) {
    ArenaAllocator arena(4096);
    auto* arr = arena.AllocateArray<int>(10);
    EXPECT_NE(arr, nullptr);

    // Should be writable
    for (int i = 0; i < 10; ++i) {
        arr[i] = i * i;
    }
    EXPECT_EQ(arr[5], 25);
}

TEST(ArenaAllocator, ResetRestoresState) {
    ArenaAllocator arena(1024);
    arena.Allocate(256);
    ASSERT_GT(arena.Used(), 0u);

    arena.Reset();
    EXPECT_EQ(arena.Used(), 0u);
    EXPECT_EQ(arena.Remaining(), 1024u);
}

TEST(ArenaAllocator, AllocationReturnsNullWhenExhausted) {
    ArenaAllocator arena(128);
    // First allocation should succeed
    void* ptr1 = arena.Allocate(64);
    EXPECT_NE(ptr1, nullptr);

    // Second allocation may succeed or fail depending on alignment,
    // but a definitely-too-large allocation must return nullptr
    void* ptr2 = arena.Allocate(256);
    EXPECT_EQ(ptr2, nullptr);
}

TEST(ArenaAllocator, AlignedAllocation) {
    ArenaAllocator arena(4096);
    void* ptr = arena.Allocate(16, 64);  // 64-byte aligned
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0u);
}

TEST(ArenaAllocator, MoveConstructor) {
    ArenaAllocator arena(1024);
    arena.Allocate(100);

    ArenaAllocator moved(std::move(arena));
    EXPECT_EQ(moved.Used(), 100u);
    EXPECT_EQ(moved.Capacity(), 1024u);
}

TEST(ArenaAllocator, MoveAssignment) {
    ArenaAllocator arena(512);
    arena.Allocate(200);

    ArenaAllocator target(1024);
    target = std::move(arena);

    EXPECT_EQ(target.Used(), 200u);
    EXPECT_EQ(target.Capacity(), 512u);
}
