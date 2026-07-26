#pragma once

#include "libengine/common/defs.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace swiftlist::indexer {

// USN reason flags.
inline constexpr uint32_t kReasonDataOverwrite     = 0x00000001;
inline constexpr uint32_t kReasonDataExtend        = 0x00000002;
inline constexpr uint32_t kReasonDataTruncation    = 0x00000004;
inline constexpr uint32_t kReasonFileCreate        = 0x00000100;
inline constexpr uint32_t kReasonFileDelete        = 0x00000200;
inline constexpr uint32_t kReasonRenameOldName     = 0x00001000;
inline constexpr uint32_t kReasonRenameNewName     = 0x00002000;
inline constexpr uint32_t kReasonBasicInfoChange   = 0x00008000;
inline constexpr uint32_t kReasonHardLinkChange    = 0x00010000;
inline constexpr uint32_t kReasonCompressionChange = 0x00020000;
inline constexpr uint32_t kReasonEncryptionChange  = 0x00040000;
inline constexpr uint32_t kReasonClose             = 0x80000000;

// File attribute flags from USN record.
inline constexpr uint32_t kFileAttrDirectory = 0x00000010;

struct UsnRecord {
    UInt128 FileReferenceNumber;
    UInt128 ParentFileReferenceNumber;
    int64_t Usn;
    uint32_t Reason;
    uint32_t FileAttributes;
    std::string FileName; // UTF-8
    uint16_t MajorVersion;
};

// Parses a single USN_RECORD from a buffer.
// Supports V2 (NTFS, 64-bit FRN) and V3 (ReFS, 128-bit FRN).
// Returns false on malformed/unsupported record.
bool ParseUsnRecord(std::span<const uint8_t> data, UsnRecord& out);

} // namespace swiftlist::indexer
