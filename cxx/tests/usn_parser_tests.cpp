#include "indexer/usn_record_parser.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

using namespace swiftlist::indexer;

TEST(UsnRecordParserTest, ParseV2) {
    std::string fileName = "test.txt";
    uint16_t fnLen = static_cast<uint16_t>(fileName.size() * 2);
    uint32_t recordLen = 60 + fnLen;

    std::vector<uint8_t> buf(recordLen, 0);
    std::memcpy(buf.data(), &recordLen, 4);
    uint16_t major = 2, minor = 0;
    std::memcpy(buf.data() + 4, &major, 2);
    std::memcpy(buf.data() + 6, &minor, 2);

    uint64_t frn = 0x123456789ABCDEF0ULL;
    std::memcpy(buf.data() + 8, &frn, 8);
    uint64_t parentFrn = 0xFEDCBA9876543210ULL;
    std::memcpy(buf.data() + 16, &parentFrn, 8);
    int64_t usn = 0x1122334455667788LL;
    std::memcpy(buf.data() + 24, &usn, 8);
    uint32_t reason = kReasonFileCreate;
    std::memcpy(buf.data() + 40, &reason, 4);
    uint32_t attrs = kFileAttrDirectory;
    std::memcpy(buf.data() + 52, &attrs, 4);
    std::memcpy(buf.data() + 56, &fnLen, 2);
    uint16_t fnOff = 60;
    std::memcpy(buf.data() + 58, &fnOff, 2);

    for (size_t i = 0; i < fileName.size(); ++i) {
        buf[60 + i * 2] = static_cast<uint8_t>(fileName[i]);
    }

    UsnRecord record;
    EXPECT_TRUE(ParseUsnRecord(buf, record));
    EXPECT_EQ(record.MajorVersion, 2);
    EXPECT_EQ(record.FileReferenceNumber.low, frn);
    EXPECT_EQ(record.FileReferenceNumber.high, 0);
    EXPECT_EQ(record.ParentFileReferenceNumber.low, parentFrn);
    EXPECT_EQ(record.Usn, usn);
    EXPECT_EQ(record.Reason, reason);
    EXPECT_EQ(record.FileAttributes, attrs);
    EXPECT_EQ(record.FileName, fileName);
}

TEST(UsnRecordParserTest, ParseV3) {
    std::string fileName = "refs_file";
    uint16_t fnLen = static_cast<uint16_t>(fileName.size() * 2);
    uint32_t recordLen = 76 + fnLen;

    std::vector<uint8_t> buf(recordLen, 0);
    std::memcpy(buf.data(), &recordLen, 4);
    uint16_t major = 3, minor = 0;
    std::memcpy(buf.data() + 4, &major, 2);
    std::memcpy(buf.data() + 6, &minor, 2);

    uint64_t frnLow = 0xAAAAAAAAAAAAAAAULL;
    uint64_t frnHigh = 0xBBBBBBBBBBBBBBBULL;
    std::memcpy(buf.data() + 8, &frnLow, 8);
    std::memcpy(buf.data() + 16, &frnHigh, 8);
    uint64_t parentLow = 0xCCCCCCCCCCCCCCCULL;
    uint64_t parentHigh = 0xDDDDDDDDDDDDDDDULL;
    std::memcpy(buf.data() + 24, &parentLow, 8);
    std::memcpy(buf.data() + 32, &parentHigh, 8);
    int64_t usn = 0x1122334455667788LL;
    std::memcpy(buf.data() + 40, &usn, 8);
    uint32_t reason = kReasonFileCreate;
    std::memcpy(buf.data() + 56, &reason, 4);
    uint32_t attrs = 0;
    std::memcpy(buf.data() + 68, &attrs, 4);
    std::memcpy(buf.data() + 72, &fnLen, 2);
    uint16_t fnOff = 76;
    std::memcpy(buf.data() + 74, &fnOff, 2);

    for (size_t i = 0; i < fileName.size(); ++i) {
        buf[76 + i * 2] = static_cast<uint8_t>(fileName[i]);
    }

    UsnRecord record;
    EXPECT_TRUE(ParseUsnRecord(buf, record));
    EXPECT_EQ(record.MajorVersion, 3);
    EXPECT_EQ(record.FileReferenceNumber.low, frnLow);
    EXPECT_EQ(record.FileReferenceNumber.high, frnHigh);
    EXPECT_EQ(record.ParentFileReferenceNumber.low, parentLow);
    EXPECT_EQ(record.ParentFileReferenceNumber.high, parentHigh);
    EXPECT_EQ(record.Usn, usn);
    EXPECT_EQ(record.FileName, fileName);
}

TEST(UsnRecordParserTest, UnsupportedVersion) {
    uint8_t buf[80] = {};
    uint32_t len = 80;
    std::memcpy(buf, &len, 4);
    uint16_t major = 4;
    std::memcpy(buf + 4, &major, 2);

    UsnRecord record;
    EXPECT_FALSE(ParseUsnRecord(buf, record));
}

TEST(UsnRecordParserTest, RecordTooShort) {
    uint8_t buf[4] = {};
    UsnRecord record;
    EXPECT_FALSE(ParseUsnRecord(buf, record));
}

TEST(UsnRecordParserTest, FileNameOffsetBeyondRecord) {
    std::vector<uint8_t> buf(64, 0);
    uint32_t len = 64;
    std::memcpy(buf.data(), &len, 4);
    uint16_t major = 2;
    std::memcpy(buf.data() + 4, &major, 2);
    uint32_t reason = kReasonFileCreate;
    std::memcpy(buf.data() + 40, &reason, 4);
    uint16_t fnLen = 10;
    std::memcpy(buf.data() + 56, &fnLen, 2);
    uint16_t fnOff = 100; // beyond record.
    std::memcpy(buf.data() + 58, &fnOff, 2);

    UsnRecord record;
    EXPECT_TRUE(ParseUsnRecord(buf, record));
    EXPECT_TRUE(record.FileName.empty());
}

TEST(UsnRecordParserTest, DirectoryAttribute) {
    std::vector<uint8_t> buf(64, 0);
    uint32_t len = 64;
    std::memcpy(buf.data(), &len, 4);
    uint16_t major = 2;
    std::memcpy(buf.data() + 4, &major, 2);

    uint16_t fnLen = 4;
    std::memcpy(buf.data() + 56, &fnLen, 2);
    uint16_t fnOff = 60;
    std::memcpy(buf.data() + 58, &fnOff, 2);
    buf[60] = 'a'; buf[62] = 'b';

    uint32_t attrs = kFileAttrDirectory;
    std::memcpy(buf.data() + 52, &attrs, 4);

    UsnRecord record;
    EXPECT_TRUE(ParseUsnRecord(buf, record));
    EXPECT_EQ(record.FileAttributes & kFileAttrDirectory, kFileAttrDirectory);
}
