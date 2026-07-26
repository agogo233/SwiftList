#include <common/utf8.h>
#include <gtest/gtest.h>

using namespace swiftlist;

// ============================================================================
// UTF-8 Conversion Tests
// ============================================================================

TEST(Utf8, RoundTripAscii) {
    std::string utf8 = "Hello, World!";
    std::wstring wide = Utf8ToWide(utf8);
    std::string back = WideToUtf8(wide);

    EXPECT_EQ(back, utf8);
}

TEST(Utf8, RoundTripChinese) {
    // "你好世界" in UTF-8
    std::string utf8 = "\xE4\xBD\xA0\xE5\xA5\xBD\xE4\xB8\x96\xE7\x95\x8C";
    std::wstring wide = Utf8ToWide(utf8);
    std::string back = WideToUtf8(wide);

    EXPECT_EQ(back, utf8);
}

TEST(Utf8, RoundTripMixed) {
    // "File_文件_123"
    std::string utf8 = "File_\xE6\x96\x87\xE4\xBB\xB6_123";
    std::wstring wide = Utf8ToWide(utf8);
    std::string back = WideToUtf8(wide);

    EXPECT_EQ(back, utf8);
}

TEST(Utf8, EmptyString) {
    EXPECT_EQ(Utf8ToWide(""), std::wstring{});
    EXPECT_EQ(WideToUtf8(L""), std::string{});
}

TEST(Utf8, IsValidUtf8) {
    EXPECT_TRUE(IsValidUtf8("Hello"));
    EXPECT_TRUE(IsValidUtf8("\xC3\xA9"));      // é
    EXPECT_TRUE(IsValidUtf8("\xE4\xB8\xAD"));   // 中

    // Invalid: lone continuation byte
    EXPECT_FALSE(IsValidUtf8("\x80"));
    // Invalid: truncated multi-byte sequence
    EXPECT_FALSE(IsValidUtf8("\xC3"));
}

TEST(Utf8, AsciiToLowerInPlace) {
    std::string s = "Hello WORLD 123";
    AsciiToLowerInPlace(s);
    EXPECT_EQ(s, "hello world 123");
}

TEST(Utf8, AsciiCaseInsensitiveEquals) {
    EXPECT_TRUE(AsciiCaseInsensitiveEquals("Hello", "hello"));
    EXPECT_TRUE(AsciiCaseInsensitiveEquals("PATH", "path"));
    EXPECT_TRUE(AsciiCaseInsensitiveEquals("", ""));
    EXPECT_FALSE(AsciiCaseInsensitiveEquals("Hello", "World"));
    EXPECT_FALSE(AsciiCaseInsensitiveEquals("Hi", "Hello"));
}
