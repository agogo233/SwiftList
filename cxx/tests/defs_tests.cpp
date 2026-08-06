#include <common/defs.h>
#include <gtest/gtest.h>

using namespace swiftlist;

// ============================================================================
// UInt128 Tests
// ============================================================================

TEST(UInt128, DefaultConstructedIsZero) {
    UInt128 v;
    EXPECT_EQ(v.low, 0u);
    EXPECT_EQ(v.high, 0u);
}

TEST(UInt128, ConstructFromParts) {
    UInt128 v{0x123456789ABCDEF0u, 0xFEDCBA9876543210u};
    EXPECT_EQ(v.low, 0x123456789ABCDEF0u);
    EXPECT_EQ(v.high, 0xFEDCBA9876543210u);
}

TEST(UInt128, Equality) {
    UInt128 a{1, 2};
    UInt128 b{1, 2};
    UInt128 c{1, 3};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(UInt128, Comparison) {
    UInt128 small{1, 0};
    UInt128 large{0, 1};  // high dominates

    EXPECT_LT(small, large);
    EXPECT_GT(large, small);
}

TEST(UInt128, Is64Bit) {
    UInt128 ntfs{42, 0};
    UInt128 refs{42, 1};

    EXPECT_TRUE(ntfs.Is64Bit());
    EXPECT_FALSE(refs.Is64Bit());
}

TEST(UInt128, HashIsDeterministic) {
    UInt128 v{0xDEADBEEFCAFEBABEu, 0x0123456789ABCDEFu};
    std::hash<UInt128> hasher;

    EXPECT_EQ(hasher(v), hasher(v));  // Same input = same hash
}

TEST(UInt128, HashDiffersForDifferentValues) {
    UInt128 a{1, 0};
    UInt128 b{2, 0};
    std::hash<UInt128> hasher;

    // Very unlikely to collide for such simple values
    EXPECT_NE(hasher(a), hasher(b));
}

TEST(UInt128, CanBeUsedAsMapKey) {
    std::unordered_map<UInt128, std::string> map;
    UInt128 key1{100, 0};
    UInt128 key2{200, 0};

    map[key1] = "alpha";
    map[key2] = "beta";

    EXPECT_EQ(map[key1], "alpha");
    EXPECT_EQ(map[key2], "beta");
    EXPECT_EQ(map.size(), 2u);
}

// ============================================================================
// DriveLetter Tests
// ============================================================================

TEST(DriveLetter, ValidLetters) {
    EXPECT_TRUE(DriveLetter{L'C'}.IsValid());
    EXPECT_TRUE(DriveLetter{L'Z'}.IsValid());
    EXPECT_TRUE(DriveLetter{L'a'}.IsValid());  // lowercase also accepted
}

TEST(DriveLetter, InvalidLetters) {
    EXPECT_FALSE(DriveLetter{L'\0'}.IsValid());
    EXPECT_FALSE(DriveLetter{L'1'}.IsValid());
    EXPECT_FALSE(DriveLetter{L'@'}.IsValid());
    EXPECT_FALSE(DriveLetter{L'['}.IsValid());  // past 'Z'
}

TEST(DriveLetter, Upper) {
    EXPECT_EQ(DriveLetter{L'c'}.Upper(), L'C');
    EXPECT_EQ(DriveLetter{L'C'}.Upper(), L'C');
}

TEST(DriveLetter, RootPath) {
    EXPECT_EQ(DriveLetter{L'C'}.RootPath(), L"C:\\");
    EXPECT_EQ(DriveLetter{L'd'}.RootPath(), L"D:\\");
}

TEST(DriveLetter, DosDevicePath) {
    EXPECT_EQ(DriveLetter{L'C'}.DosDevicePath(), L"\\\\.\\C:");
}

// ============================================================================
// FileAttributes Tests
// ============================================================================

TEST(FileAttributes, HasFlag) {
    auto attrs = FileAttributes::Directory | FileAttributes::Hidden;

    EXPECT_TRUE(HasFlag(attrs, FileAttributes::Directory));
    EXPECT_TRUE(HasFlag(attrs, FileAttributes::Hidden));
    EXPECT_FALSE(HasFlag(attrs, FileAttributes::ReadOnly));
    EXPECT_FALSE(HasFlag(attrs, FileAttributes::System));
}

TEST(FileAttributes, OrAssignment) {
    FileAttributes a = FileAttributes::ReadOnly;
    a |= FileAttributes::Hidden;

    EXPECT_TRUE(HasFlag(a, FileAttributes::ReadOnly));
    EXPECT_TRUE(HasFlag(a, FileAttributes::Hidden));
}

TEST(FileAttributes, NoneHasNoFlags) {
    EXPECT_FALSE(HasFlag(FileAttributes::None, FileAttributes::ReadOnly));
    EXPECT_FALSE(HasFlag(FileAttributes::None, FileAttributes::Directory));
}
