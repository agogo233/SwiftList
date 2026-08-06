#include "pipe/wire_format.h"
#include "pipe/search_request_wire.h"
#include "pipe/search_response_wire.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace swiftlist::pipe;

// --- 7-bit varint round-trip ---

TEST(WireFormatTest, VarIntSmall) {
    uint8_t buf[5] = {};
    auto r = Write7BitEncodedInt(std::span<uint8_t>(buf, 5), 0);
    EXPECT_EQ(r.bytesWritten, 1);
    EXPECT_FALSE(r.overflow);

    size_t off = 0;
    EXPECT_EQ(Read7BitEncodedInt(std::span<const uint8_t>(buf, r.bytesWritten), off), 0);
}

TEST(WireFormatTest, VarIntMedium) {
    uint8_t buf[5] = {};
    int32_t val = 300;
    auto r = Write7BitEncodedInt(std::span<uint8_t>(buf, 5), val);
    EXPECT_EQ(r.bytesWritten, 2);

    size_t off = 0;
    EXPECT_EQ(Read7BitEncodedInt(std::span<const uint8_t>(buf, r.bytesWritten), off), val);
}

TEST(WireFormatTest, VarIntMax) {
    uint8_t buf[5] = {};
    int32_t val = INT32_MAX;
    auto r = Write7BitEncodedInt(std::span<uint8_t>(buf, 5), val);
    EXPECT_EQ(r.bytesWritten, 5);

    size_t off = 0;
    EXPECT_EQ(Read7BitEncodedInt(std::span<const uint8_t>(buf, r.bytesWritten), off), val);
}

TEST(WireFormatTest, VarIntOverflow) {
    uint8_t buf[1] = {};
    auto r = Write7BitEncodedInt(std::span<uint8_t>(buf, 1), 200);
    EXPECT_TRUE(r.overflow);
}

// --- String round-trip ---

TEST(WireFormatTest, StringRoundTrip) {
    std::vector<uint8_t> buf;
    std::string input = "hello world";
    WriteString(buf, input);

    size_t off = 0;
    auto s = ReadString(std::span<const uint8_t>(buf.data(), buf.size()), off);
    EXPECT_EQ(s, input);
}

TEST(WireFormatTest, StringEmpty) {
    std::vector<uint8_t> buf;
    WriteString(buf, "");

    size_t off = 0;
    auto s = ReadString(std::span<const uint8_t>(buf.data(), buf.size()), off);
    EXPECT_TRUE(s.empty());
}

// --- Int32/UInt64 round trip ---

TEST(WireFormatTest, Int32RoundTrip) {
    uint8_t buf[8] = {};
    WriteInt32LE(std::span<uint8_t>(buf, 4), -12345);
    WriteInt32LE(std::span<uint8_t>(buf + 4, 4), 0);

    size_t off = 0;
    EXPECT_EQ(ReadInt32LE(std::span<const uint8_t>(buf, 4), off), -12345);
    EXPECT_EQ(ReadInt32LE(std::span<const uint8_t>(buf + 4, 4), off), 0);
}

TEST(WireFormatTest, UInt64RoundTrip) {
    uint8_t buf[8] = {};
    uint64_t val = 0xDEADBEEFCAFEBABEULL;
    WriteUInt64LE(std::span<uint8_t>(buf, 8), val);

    size_t off = 0;
    EXPECT_EQ(ReadUInt64LE(std::span<const uint8_t>(buf, 8), off), val);
}

// --- SearchRequest round trip ---

TEST(SearchRequestWireTest, PingRoundTrip) {
    SearchRequestMessage original;
    original.Id = SearchRequestId::Ping;

    std::vector<uint8_t> buf;
    WriteSearchRequest(buf, original);

    // Verify frame header.
    size_t off = 0;
    EXPECT_EQ(ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4)), kRequestMagic);
    off += 4;
    EXPECT_EQ(ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4)), kRequestVersion);
    off += 4;
    auto payloadLen = ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4));
    off += 4;

    SearchRequestMessage decoded;
    EXPECT_TRUE(ReadSearchRequest(buf.data() + off, payloadLen, decoded));
    EXPECT_EQ(decoded.Id, SearchRequestId::Ping);
}

TEST(SearchRequestWireTest, SearchRoundTrip) {
    SearchRequestMessage original;
    original.Id = SearchRequestId::Search;
    original.Limit = 50;
    original.AppLimit = 10;
    original.Query = "test query";
    original.ExactMatch = true;
    std::vector<std::string> disabled = {"comp1", "comp2"};
    original.DisabledAliasComponents = std::make_unique<std::vector<std::string>>(disabled);

    std::vector<uint8_t> buf;
    WriteSearchRequest(buf, original);

    size_t off = 0;
    off += 4; // magic
    EXPECT_EQ(ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4)), kRequestVersion);
    off += 4; // version
    auto payloadLen = ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4));
    off += 4;

    SearchRequestMessage decoded;
    EXPECT_TRUE(ReadSearchRequest(buf.data() + off, payloadLen, decoded));
    EXPECT_EQ(decoded.Id, SearchRequestId::Search);
    EXPECT_EQ(decoded.Limit, 50);
    EXPECT_EQ(decoded.AppLimit, 10);
    EXPECT_EQ(decoded.Query, "test query");
    EXPECT_TRUE(decoded.ExactMatch);
    ASSERT_NE(decoded.DisabledAliasComponents, nullptr);
    ASSERT_EQ(decoded.DisabledAliasComponents->size(), 2);
    EXPECT_EQ((*decoded.DisabledAliasComponents)[0], "comp1");
    EXPECT_EQ((*decoded.DisabledAliasComponents)[1], "comp2");
}

TEST(SearchRequestWireTest, SearchDirRoundTrip) {
    SearchRequestMessage original;
    original.Id = SearchRequestId::SearchDir;
    original.Limit = 100;
    original.AppLimit = 20;
    original.DirectoryFilter = "C:\\Windows";
    original.Query = "file.txt";
    original.ExactMatch = false;

    std::vector<uint8_t> buf;
    WriteSearchRequest(buf, original);

    size_t off = 0;
    off += 4;
    EXPECT_EQ(ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4)), kRequestVersion);
    off += 4;
    auto payloadLen = ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4));
    off += 4;

    SearchRequestMessage decoded;
    EXPECT_TRUE(ReadSearchRequest(buf.data() + off, payloadLen, decoded));
    EXPECT_EQ(decoded.Id, SearchRequestId::SearchDir);
    EXPECT_EQ(decoded.Limit, 100);
    EXPECT_EQ(decoded.DirectoryFilter, "C:\\Windows");
    EXPECT_EQ(decoded.Query, "file.txt");
    EXPECT_FALSE(decoded.ExactMatch);
}

TEST(SearchRequestWireTest, LaunchHookRoundTrip) {
    SearchRequestMessage original;
    original.Id = SearchRequestId::LaunchHook;
    original.RequestElevation = true;

    std::vector<uint8_t> buf;
    WriteSearchRequest(buf, original);

    size_t off = 0;
    off += 8;
    auto payloadLen = ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4));
    off += 4;

    SearchRequestMessage decoded;
    EXPECT_TRUE(ReadSearchRequest(buf.data() + off, payloadLen, decoded));
    EXPECT_EQ(decoded.Id, SearchRequestId::LaunchHook);
    EXPECT_TRUE(decoded.RequestElevation);
}

TEST(SearchRequestWireTest, EnumerateDirRoundTrip) {
    SearchRequestMessage original;
    original.Id = SearchRequestId::EnumerateDir;
    original.Limit = 50;
    original.DirectoryFilter = "C:\\Users";
    original.Query = "*.txt";
    original.Recursive = true;

    std::vector<uint8_t> buf;
    WriteSearchRequest(buf, original);

    size_t off = 0;
    off += 4;
    EXPECT_EQ(ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4)), kRequestVersion);
    off += 4;
    auto payloadLen = ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4));
    off += 4;

    SearchRequestMessage decoded;
    EXPECT_TRUE(ReadSearchRequest(buf.data() + off, payloadLen, decoded));
    EXPECT_EQ(decoded.Id, SearchRequestId::EnumerateDir);
    EXPECT_EQ(decoded.Limit, 50);
    EXPECT_EQ(decoded.DirectoryFilter, "C:\\Users");
    EXPECT_EQ(decoded.Query, "*.txt");
    EXPECT_TRUE(decoded.Recursive);
}

TEST(SearchRequestWireTest, CancelDriveIndexRoundTrip) {
    SearchRequestMessage original;
    original.Id = SearchRequestId::CancelDriveIndex;
    original.Drive = "D:";

    std::vector<uint8_t> buf;
    WriteSearchRequest(buf, original);

    size_t off = 0;
    off += 8;
    auto payloadLen = ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4));
    off += 4;

    SearchRequestMessage decoded;
    EXPECT_TRUE(ReadSearchRequest(buf.data() + off, payloadLen, decoded));
    EXPECT_EQ(decoded.Id, SearchRequestId::CancelDriveIndex);
    EXPECT_EQ(decoded.Drive, "D:");
}

TEST(SearchRequestWireTest, SubscribeDirectoryChangesRoundTrip) {
    SearchRequestMessage original;
    original.Id = SearchRequestId::SubscribeDirectoryChanges;
    std::vector<std::string> dirs = {"C:\\Users", "D:\\Data"};
    original.Directories = std::make_unique<std::vector<std::string>>(dirs);

    std::vector<uint8_t> buf;
    WriteSearchRequest(buf, original);

    size_t off = 0;
    off += 8;
    auto payloadLen = ReadInt32LE(std::span<const uint8_t>(buf.data() + off, 4));
    off += 4;

    SearchRequestMessage decoded;
    EXPECT_TRUE(ReadSearchRequest(buf.data() + off, payloadLen, decoded));
    EXPECT_EQ(decoded.Id, SearchRequestId::SubscribeDirectoryChanges);
    ASSERT_NE(decoded.Directories, nullptr);
    ASSERT_EQ(decoded.Directories->size(), 2);
    EXPECT_EQ((*decoded.Directories)[0], "C:\\Users");
    EXPECT_EQ((*decoded.Directories)[1], "D:\\Data");
}

// --- SearchResponse round trip ---

TEST(SearchResponseWireTest, WriteAndReadStream) {
    std::vector<uint8_t> buf;
    WriteSearchResponseHeader(buf);

    SearchResult r1;
    r1.Name = "file1.txt";
    r1.Path = "C:\\test\\file1.txt";
    r1.IsDir = false;
    r1.Drive = "C:";
    r1.RankSortKey = 12345;
    r1.Metadata.Size = 1024;
    r1.Metadata.CreatedUnix = 1609459200;
    r1.Metadata.ModifiedUnix = 1609459300;
    r1.Metadata.AccessedUnix = 1609459400;
    r1.Attributes = 32; // FILE_ATTRIBUTE_ARCHIVE
    WriteSearchResponseFileResult(buf, r1);

    SearchResult r2;
    r2.Name = "dir1";
    r2.Path = "C:\\test\\dir1";
    r2.IsDir = true;
    r2.Drive = "C:";
    r2.RankSortKey = 12346;
    r2.Metadata = {0, 0, 0, 0};
    r2.Attributes = 16; // FILE_ATTRIBUTE_DIRECTORY
    WriteSearchResponseFileResult(buf, r2);

    WriteSearchResponseEnd(buf);

    // Read back.
    std::vector<SearchResult> results;
    EXPECT_TRUE(ReadSearchResponseStream(buf.data(), buf.size(),
                                        [&](const SearchResult& r) {
                                            results.push_back(r);
                                        }));

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].Name, "file1.txt");
    EXPECT_EQ(results[0].Path, "C:\\test\\file1.txt");
    EXPECT_FALSE(results[0].IsDir);
    EXPECT_EQ(results[0].Drive, "C:");
    EXPECT_EQ(results[0].RankSortKey, 12345);
    EXPECT_EQ(results[0].Metadata.Size, 1024);
    EXPECT_EQ(results[0].Metadata.CreatedUnix, 1609459200);
    EXPECT_EQ(results[0].Attributes, 32);

    EXPECT_EQ(results[1].Name, "dir1");
    EXPECT_TRUE(results[1].IsDir);
    EXPECT_EQ(results[1].Attributes, 16);
}
