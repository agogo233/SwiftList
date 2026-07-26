#include "indexer/usn_record_parser.h"

#include <cstring>

namespace swiftlist::indexer {

namespace {

uint64_t ReadLE(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n && i < 8; ++i) {
        v |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    return v;
}

} // namespace

bool ParseUsnRecord(std::span<const uint8_t> data, UsnRecord& out) {
    if (data.size() < 8) return false;

    uint32_t recordLength = static_cast<uint32_t>(ReadLE(data.data(), 4));
    if (recordLength < 8 || static_cast<size_t>(recordLength) > data.size()) return false;

    uint16_t majorVersion = static_cast<uint16_t>(ReadLE(data.data() + 4, 2));
    out.MajorVersion = majorVersion;

    if (majorVersion == 2) {
        // USN_RECORD_V2: 64-bit FRN.
        if (recordLength < 60) return false;

        out.FileReferenceNumber = {ReadLE(data.data() + 8, 8), 0};
        out.ParentFileReferenceNumber = {ReadLE(data.data() + 16, 8), 0};
        out.Usn = static_cast<int64_t>(ReadLE(data.data() + 24, 8));
        out.Reason = static_cast<uint32_t>(ReadLE(data.data() + 40, 4));
        out.FileAttributes = static_cast<uint32_t>(ReadLE(data.data() + 52, 4));

        uint16_t fileNameLength = static_cast<uint16_t>(ReadLE(data.data() + 56, 2));
        uint16_t fileNameOffset = static_cast<uint16_t>(ReadLE(data.data() + 58, 2));

        if (fileNameOffset + fileNameLength <= recordLength) {
            // Name is UTF-16LE bytes.
            const auto* nameBytes = data.data() + fileNameOffset;
            size_t nameChars = fileNameLength / 2;

            // Convert UTF-16LE to UTF-8.
            out.FileName.clear();
            out.FileName.reserve(nameChars);
            for (size_t i = 0; i < nameChars; ++i) {
                char16_t c = static_cast<char16_t>(ReadLE(nameBytes + i * 2, 2));
                if (c < 0x80) {
                    out.FileName.push_back(static_cast<char>(c));
                } else if (c < 0x800) {
                    out.FileName.push_back(static_cast<char>(0xC0 | (c >> 6)));
                    out.FileName.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                } else {
                    out.FileName.push_back(static_cast<char>(0xE0 | (c >> 12)));
                    out.FileName.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                    out.FileName.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
            }
        }
        return true;

    } else if (majorVersion == 3) {
        // USN_RECORD_V3: 128-bit FRN.
        if (recordLength < 76) return false;

        out.FileReferenceNumber = {ReadLE(data.data() + 8, 8), ReadLE(data.data() + 16, 8)};
        out.ParentFileReferenceNumber = {ReadLE(data.data() + 24, 8), ReadLE(data.data() + 32, 8)};
        out.Usn = static_cast<int64_t>(ReadLE(data.data() + 40, 8));
        out.Reason = static_cast<uint32_t>(ReadLE(data.data() + 56, 4));
        out.FileAttributes = static_cast<uint32_t>(ReadLE(data.data() + 68, 4));

        uint16_t fileNameLength = static_cast<uint16_t>(ReadLE(data.data() + 72, 2));
        uint16_t fileNameOffset = static_cast<uint16_t>(ReadLE(data.data() + 74, 2));

        if (fileNameOffset + fileNameLength <= recordLength) {
            const auto* nameBytes = data.data() + fileNameOffset;
            size_t nameChars = fileNameLength / 2;
            out.FileName.clear();
            out.FileName.reserve(nameChars);
            for (size_t i = 0; i < nameChars; ++i) {
                char16_t c = static_cast<char16_t>(ReadLE(nameBytes + i * 2, 2));
                if (c < 0x80) {
                    out.FileName.push_back(static_cast<char>(c));
                } else if (c < 0x800) {
                    out.FileName.push_back(static_cast<char>(0xC0 | (c >> 6)));
                    out.FileName.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                } else {
                    out.FileName.push_back(static_cast<char>(0xE0 | (c >> 12)));
                    out.FileName.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                    out.FileName.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
            }
        }
        return true;
    }

    return false; // unsupported version
}

} // namespace swiftlist::indexer
