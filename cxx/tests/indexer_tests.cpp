#include "indexer/mft_parser.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

using namespace swiftlist::indexer;

TEST(MftParserTest, ReadLE) {
    uint8_t buf[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(ReadLE(buf, 1), 1);
    EXPECT_EQ(ReadLE(buf, 2), 0x0201);
    EXPECT_EQ(ReadLE(buf, 3), 0x030201);
    EXPECT_EQ(ReadLE(buf, 4), 0x04030201);
}

TEST(MftParserTest, ReadSignedLE) {
    uint8_t pos[] = {0x7F, 0x00};
    EXPECT_EQ(ReadSignedLE(pos, 2), 127);

    uint8_t neg[] = {0xFF, 0xFF};
    EXPECT_EQ(ReadSignedLE(neg, 2), -1);

    uint8_t neg4[] = {0x00, 0x00, 0x00, 0x80};
    EXPECT_EQ(ReadSignedLE(neg4, 4), static_cast<int64_t>(0x80000000));
}

TEST(MftParserTest, FileTimeToUnix) {
    EXPECT_EQ(FileTimeToUnixSeconds(116444736000000000LL), 0);

    int64_t ft2020 = 116444736000000000LL + 1577836800LL * 10'000'000LL;
    EXPECT_EQ(FileTimeToUnixSeconds(ft2020), 1577836800);

    EXPECT_EQ(FileTimeToUnixSeconds(0), 0);
    EXPECT_EQ(FileTimeToUnixSeconds(-1), 0);
}

TEST(MftParserTest, ApplyFixup) {
    constexpr uint32_t kSectorSize = 512;
    constexpr size_t kRecordSize = kSectorSize * 2;

    std::vector<uint8_t> buf(kRecordSize, 0);
    buf[0] = 'F'; buf[1] = 'I'; buf[2] = 'L'; buf[3] = 'E';

    buf[4] = 0x30; buf[5] = 0x00; // USA offset = 0x30.
    buf[6] = 0x03; buf[7] = 0x00; // USA count = 3.

    buf[0x30] = 0xCD; buf[0x31] = 0xAB; // Placeholder.
    buf[0x32] = 0x34; buf[0x33] = 0x12; // Sector 0 tail.
    buf[0x34] = 0x78; buf[0x35] = 0x56; // Sector 1 tail.

    buf[kSectorSize - 2] = 0x00; buf[kSectorSize - 1] = 0x00;
    buf[2 * kSectorSize - 2] = 0x00; buf[2 * kSectorSize - 1] = 0x00;

    EXPECT_TRUE(ApplyFixup(buf, kSectorSize));

    EXPECT_EQ(buf[kSectorSize - 2], 0x34);
    EXPECT_EQ(buf[kSectorSize - 1], 0x12);
    EXPECT_EQ(buf[2 * kSectorSize - 2], 0x78);
    EXPECT_EQ(buf[2 * kSectorSize - 1], 0x56);
}

TEST(MftParserTest, ParseDataRunsSingleRun) {
    uint8_t runs[] = {0x11, 0x05, 0x0A, 0x00};
    auto result = ParseDataRuns(runs);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].Lcn, 10);
    EXPECT_EQ(result[0].ClusterCount, 5);
}

TEST(MftParserTest, ParseDataRunsMultipleRuns) {
    uint8_t runs[] = {
        0x21, 0x05, 0x10,
        0x21, 0x03, 0x0A,
        0x00
    };
    auto result = ParseDataRuns(runs);
    ASSERT_GE(result.size(), 2);
    EXPECT_EQ(result[0].Lcn, 16);
    EXPECT_EQ(result[0].ClusterCount, 5);
    EXPECT_EQ(result[1].Lcn, 26);
    EXPECT_EQ(result[1].ClusterCount, 3);
}

TEST(MftParserTest, ParseDataRunsNegativeDelta) {
    uint8_t runs[] = {
        0x11, 0x05, 0x64,
        0x21, 0x03, 0xCE, 0xFF,
        0x00
    };
    auto result = ParseDataRuns(runs);
    ASSERT_GE(result.size(), 2);
    EXPECT_EQ(result[0].Lcn, 100);
    EXPECT_EQ(result[0].ClusterCount, 5);
    EXPECT_EQ(result[1].Lcn, 50);
    EXPECT_EQ(result[1].ClusterCount, 3);
}

TEST(MftParserTest, ParseDataRunsEmpty) {
    EXPECT_TRUE(ParseDataRuns({}).empty());
}

TEST(MftParserTest, ParseDataRunsEndMarkerOnly) {
    uint8_t runs[] = {0x00};
    EXPECT_TRUE(ParseDataRuns(runs).empty());
}
