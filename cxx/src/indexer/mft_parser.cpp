#include "indexer/mft_parser.h"

#include <algorithm>
#include <cstring>

namespace swiftlist::indexer {

namespace {

constexpr int64_t kFileTimeToUnixOffset = 116444736000000000LL; // 1601-01-01 to 1970-01-01 in 100ns

uint64_t ReadLEImpl(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n && i < 8; ++i) {
        v |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    return v;
}

int64_t ReadSignedLEImpl(const uint8_t* p, int n) {
    int64_t v = 0;
    for (int i = 0; i < n && i < 8; ++i) {
        v |= static_cast<int64_t>(p[i]) << (8 * i);
    }
    // Sign-extend.
    if (n < 8 && (p[n - 1] & 0x80)) {
        v |= (~static_cast<int64_t>(0)) << (8 * n);
    }
    return v;
}

} // namespace

uint64_t ReadLE(const uint8_t* p, int n) {
    return ReadLEImpl(p, n);
}

int64_t ReadSignedLE(const uint8_t* p, int n) {
    return ReadSignedLEImpl(p, n);
}

uint32_t FileTimeToUnixSeconds(int64_t fileTime) {
    if (fileTime <= 0) return 0;
    int64_t unixTime = (fileTime - kFileTimeToUnixOffset) / 10'000'000LL;
    if (unixTime <= 0) return 0;
    if (unixTime > static_cast<int64_t>(UINT32_MAX)) return UINT32_MAX;
    return static_cast<uint32_t>(unixTime);
}

bool ApplyFixup(std::span<uint8_t> buf, uint32_t bytesPerSector) {
    if (bytesPerSector < 2 || buf.size() < 8) return false;

    uint16_t usaOff = static_cast<uint16_t>(ReadLE(buf.data() + 4, 2));
    uint16_t usaCount = static_cast<uint16_t>(ReadLE(buf.data() + 6, 2));

    // usaCount includes the placeholder USN at index 0.
    if (usaOff == 0 || usaCount < 2) return true; // nothing to fix

    for (uint16_t i = 1; i < usaCount; ++i) {
        size_t secEnd = static_cast<size_t>(i) * bytesPerSector - 2;
        size_t usaEntry = usaOff + i * 2;

        if (secEnd + 1 >= buf.size() || usaEntry + 1 >= buf.size()) {
            break; // out of bounds, stop
        }

        buf[secEnd]     = buf[usaEntry];
        buf[secEnd + 1] = buf[usaEntry + 1];
    }

    return true;
}

std::vector<DataRun> ParseDataRuns(std::span<const uint8_t> attributeValue) {
    std::vector<DataRun> runs;
    if (attributeValue.empty()) return runs;

    int64_t lcn = 0;
    size_t p = 0;

    while (p < attributeValue.size()) {
        uint8_t hdr = attributeValue[p++];
        if (hdr == 0) break; // end marker

        int lenBytes = hdr & 0x0F;
        int offBytes = (hdr >> 4) & 0x0F;

        if (p + lenBytes + offBytes > attributeValue.size()) break;

        uint64_t runLen = ReadLE(attributeValue.data() + p, lenBytes);
        p += lenBytes;

        int64_t runOff = ReadSignedLE(attributeValue.data() + p, offBytes);
        p += offBytes;

        lcn += runOff;
        if (runLen > 0) {
            runs.push_back({lcn, runLen});
        }
    }

    return runs;
}

std::vector<MftNameEntry> CollectNames(std::span<const uint8_t> record,
                                       uint32_t bytesPerSector,
                                       int recordIndex) {
    std::vector<MftNameEntry> names;
    if (record.size() < 24) return names;

    // Check magic.
    if (record[0] != 'F' || record[1] != 'I' || record[2] != 'L' || record[3] != 'E') {
        return names;
    }

    uint16_t flags = static_cast<uint16_t>(ReadLE(record.data() + 22, 2));
    if ((flags & 0x01) == 0) return names; // not in use

    // First attribute offset.
    uint16_t firstAttrOff = static_cast<uint16_t>(ReadLE(record.data() + 20, 2));
    size_t attrOff = firstAttrOff;
    size_t recordLen = record.size();

    // Per-record state.
    uint32_t creationUnix = 0;
    uint32_t lastWriteUnix = 0;
    uint32_t lastAccessUnix = 0;
    uint32_t fileAttrs = 0;
    uint64_t dataRealSize = UINT64_MAX; // from $DATA

    while (attrOff + 8 <= recordLen) {
        uint32_t attrType = static_cast<uint32_t>(ReadLE(record.data() + attrOff, 4));
        uint32_t attrLen = static_cast<uint32_t>(ReadLE(record.data() + attrOff + 4, 4));

        if (attrType == 0xFFFFFFFF) break; // end marker
        if (attrLen == 0 || attrOff + attrLen > recordLen) break;

        uint8_t residentFlag = record[attrOff + 8];
        uint8_t nameLen = record[attrOff + 9];
        uint16_t nameOff = static_cast<uint16_t>(ReadLE(record.data() + attrOff + 10, 2));

        if (attrType == 0x10 && residentFlag == 0) {
            // $STANDARD_INFORMATION (resident)
            uint16_t valueOff = static_cast<uint16_t>(ReadLE(record.data() + attrOff + 20, 2));
            size_t vo = attrOff + valueOff;
            if (vo + 44 <= recordLen) {
                int64_t creation = static_cast<int64_t>(ReadLE(record.data() + vo, 8));
                int64_t lastWrite = static_cast<int64_t>(ReadLE(record.data() + vo + 8, 8));
                int64_t lastAccess = static_cast<int64_t>(ReadLE(record.data() + vo + 0x18, 8));
                creationUnix = FileTimeToUnixSeconds(creation);
                lastWriteUnix = FileTimeToUnixSeconds(lastWrite);
                lastAccessUnix = FileTimeToUnixSeconds(lastAccess);
                fileAttrs = static_cast<uint32_t>(ReadLE(record.data() + vo + 0x20, 4));
            }
        } else if (attrType == 0x30 && residentFlag == 0) {
            // $FILE_NAME (resident)
            uint16_t valueOff = static_cast<uint16_t>(ReadLE(record.data() + attrOff + 20, 2));
            size_t vo = attrOff + valueOff;
            if (vo + 66 <= recordLen) {
                UInt128 parentFrn{ReadLE(record.data() + vo, 8), 0};
                uint64_t realSize = ReadLE(record.data() + vo + 0x30, 8);
                uint8_t nameLength = record[vo + 0x40];
                uint8_t nameSpace = record[vo + 0x41];

                // Skip DOS-only namespace (2).
                if (nameSpace != 2 && nameLength > 0 && vo + 0x42 + nameLength * 2 <= recordLen) {
                    // Name is UTF-16LE.
                    std::u16string_view u16name(
                        reinterpret_cast<const char16_t*>(record.data() + vo + 0x42),
                        nameLength);

                    // Convert UTF-16 to UTF-8.
                    std::string utf8Name;
                    utf8Name.reserve(nameLength);
                    for (char16_t c : u16name) {
                        if (c < 0x80) {
                            utf8Name.push_back(static_cast<char>(c));
                        } else if (c < 0x800) {
                            utf8Name.push_back(static_cast<char>(0xC0 | (c >> 6)));
                            utf8Name.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                        } else {
                            utf8Name.push_back(static_cast<char>(0xE0 | (c >> 12)));
                            utf8Name.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                            utf8Name.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                        }
                    }

                    MftNameEntry entry;
                    entry.ParentFrn = parentFrn;
                    entry.Name = std::move(utf8Name);
                    entry.FileAttributes = fileAttrs;
                    entry.RealSize = realSize;
                    entry.CreationUnix = creationUnix;
                    entry.LastWriteUnix = lastWriteUnix;
                    entry.LastAccessUnix = lastAccessUnix;
                    names.push_back(std::move(entry));
                }
            }
        } else if (attrType == 0x80) {
            // $DATA
            if (nameLen == 0) { // unnamed stream
                if (residentFlag == 0) {
                    // Non-resident: real size at +0x30 from attribute start.
                    if (attrOff + 0x30 + 8 <= recordLen) {
                        dataRealSize = ReadLE(record.data() + attrOff + 0x30, 8);
                    }
                } else {
                    // Resident: real size at +0x10.
                    uint16_t valueOff = static_cast<uint16_t>(ReadLE(record.data() + attrOff + 20, 2));
                    size_t vo = attrOff + valueOff;
                    if (vo + 8 <= recordLen) {
                        dataRealSize = ReadLE(record.data() + vo, 8);
                    }
                }
            }
        }

        attrOff += attrLen;
    }

    // Apply $DATA real size override.
    for (auto& name : names) {
        if (dataRealSize != UINT64_MAX) {
            name.RealSize = dataRealSize;
        }
    }

    return names;
}

} // namespace swiftlist::indexer
