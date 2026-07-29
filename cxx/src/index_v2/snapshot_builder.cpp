#include "index_v2/snapshot_builder.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace swiftlist::index_v2 {

namespace {

// Compute the character mask for a UTF-8 string (ASCII-only letters/digits).
uint64_t ComputeCharMask(std::string_view s) {
    uint64_t mask = 0;
    for (unsigned char c : s) {
        if (c >= 'a' && c <= 'z')
            mask |= (1ULL << (c - 'a'));
        else if (c >= 'A' && c <= 'Z')
            mask |= (1ULL << (c - 'A'));
        else if (c >= '0' && c <= '9')
            mask |= (1ULL << (26 + (c - '0')));
    }
    return mask;
}

} // namespace

SnapshotColumns SnapshotBuilder::Build(std::vector<FileRecordInput> records,
                                       const Meta& metaTemplate) {
    SnapshotColumns cols;
    cols.meta = metaTemplate;

    const int rowCount = static_cast<int>(records.size());
    cols.meta.RowCount = rowCount;

    // 1. Sort by Id (ascending, treating low then high).
    std::sort(records.begin(), records.end(),
              [](const FileRecordInput& a, const FileRecordInput& b) {
                  if (a.Id.low != b.Id.low) return a.Id.low < b.Id.low;
                  return a.Id.high < b.Id.high;
              });

    // 2. Build unique name dictionary and NameBlob.
    //    Map: uid -> original row's name index (first occurrence).
    std::unordered_map<std::string, uint32_t, std::hash<std::string>> nameToUid;
    std::vector<std::string_view> uniqueNames;
    uint32_t nextUid = 0;

    cols.nameIds.resize(rowCount);
    cols.nameOffsets.push_back(0); // NameOffsets[0] = 0

    for (int i = 0; i < rowCount; ++i) {
        const auto& name = records[i].Name;
        auto it = nameToUid.find(name);
        if (it == nameToUid.end()) {
            uint32_t uid = nextUid++;
            nameToUid[name] = uid;
            cols.nameIds[i] = uid;

            // Append to NameBlob.
            size_t offset = cols.nameBlob.size();
            size_t nameLen = name.size();
            cols.nameBlob.resize(offset + nameLen);
            std::memcpy(cols.nameBlob.data() + offset, name.data(), nameLen);
            cols.nameOffsets.push_back(static_cast<uint32_t>(cols.nameBlob.size()));

            uniqueNames.push_back(
                std::string_view(cols.nameBlob.data() + offset, nameLen));
        } else {
            cols.nameIds[i] = it->second;
        }
    }

    cols.meta.UniqueCount = static_cast<int>(nextUid);
    cols.meta.NameBlobLength = static_cast<int>(cols.nameBlob.size());

    // 3. Compute UniqueMasks.
    cols.uniqueMasks.resize(nextUid);
    for (uint32_t uid = 0; uid < nextUid; ++uid) {
        cols.uniqueMasks[uid] = ComputeCharMask(uniqueNames[uid]);
    }

    // 4. Build ID -> row index map for parent resolution.
    std::unordered_map<UInt128, int, std::hash<UInt128>> idToRow;
    idToRow.reserve(rowCount * 2);
    for (int i = 0; i < rowCount; ++i) {
        idToRow[records[i].Id] = i;
    }

    // 5. Resolve parent indexes.
    cols.parentIndexes.resize(rowCount);
    for (int i = 0; i < rowCount; ++i) {
        auto it = idToRow.find(records[i].ParentId);
        cols.parentIndexes[i] = (it != idToRow.end()) ? it->second : -1;
    }

    // 6. Sort rows within each UID group (stable: by row index).
    //    Build UidStarts and UidRows.
    cols.uidStarts.assign(nextUid + 1, 0);
    // First pass: count rows per UID.
    for (int i = 0; i < rowCount; ++i) {
        cols.uidStarts[cols.nameIds[i] + 1]++;
    }
    // Prefix sum.
    for (uint32_t uid = 0; uid < nextUid; ++uid) {
        cols.uidStarts[uid + 1] += cols.uidStarts[uid];
    }
    // Second pass: fill UidRows using the starts as insertion cursors.
    cols.uidRows.resize(rowCount);
    std::vector<int32_t> uidCursor(cols.uidStarts.begin(), cols.uidStarts.begin() + nextUid);
    for (int i = 0; i < rowCount; ++i) {
        uint32_t uid = cols.nameIds[i];
        int32_t pos = uidCursor[uid]++;
        cols.uidRows[pos] = i;
    }

    // 7. Build CSR children array.
    //    Count children per row.
    cols.childStarts.assign(rowCount + 1, 0);
    std::vector<int32_t> childCursor(rowCount + 1, 0);
    int childrenLength = 0;
    for (int i = 0; i < rowCount; ++i) {
        if (cols.parentIndexes[i] >= 0) {
            cols.childStarts[cols.parentIndexes[i] + 1]++;
        }
    }
    // Prefix sum.
    for (int i = 0; i < rowCount; ++i) {
        cols.childStarts[i + 1] += cols.childStarts[i];
    }
    childrenLength = cols.childStarts[rowCount];
    cols.meta.ChildrenLength = childrenLength;
    // Fill children.
    cols.children.resize(childrenLength);
    childCursor.assign(cols.childStarts.begin(), cols.childStarts.begin() + rowCount);
    for (int i = 0; i < rowCount; ++i) {
        int parent = cols.parentIndexes[i];
        if (parent >= 0) {
            int32_t pos = childCursor[parent]++;
            cols.children[pos] = i;
        }
    }

    // 8. Collect orphans (rows whose parent wasn't resolved).
    for (int i = 0; i < rowCount; ++i) {
        if (cols.parentIndexes[i] < 0 && !(records[i].ParentId.low == 0 && records[i].ParentId.high == 0)) {
            cols.orphanRows.push_back(i);
            cols.orphanFrns.push_back(records[i].ParentId);
        }
    }
    cols.meta.OrphanCount = static_cast<int>(cols.orphanRows.size());

    // 9. Fill simple columns (Ids, Sizes, Times).
    cols.ids.resize(rowCount);
    cols.sizes.resize(rowCount);
    cols.creationTimes.resize(rowCount);
    cols.lastWriteTimes.resize(rowCount);
    cols.lastAccessTimes.resize(rowCount);
    for (int i = 0; i < rowCount; ++i) {
        cols.ids[i] = records[i].Id;
        cols.sizes[i] = records[i].Size;
        cols.creationTimes[i] = records[i].CreationTimeUnix;
        cols.lastWriteTimes[i] = records[i].LastWriteTimeUnix;
        cols.lastAccessTimes[i] = records[i].LastAccessTimeUnix;
        // nameIds[i] already set above
        cols.flags.push_back(static_cast<uint16_t>(records[i].Flags));
    }

    // 10. Compute UniqueAsciiBits.
    int asciiWords = (static_cast<int>(nextUid) + 63) / 64;
    cols.uniqueAsciiBits.assign(asciiWords, 0);
    for (uint32_t uid = 0; uid < nextUid; ++uid) {
        // A name is ASCII-only if its byte length equals its char count (no multi-byte).
        // Simple check: all bytes < 0x80.
        auto sv = uniqueNames[uid];
        bool isAscii = true;
        for (unsigned char c : sv) {
            if (c >= 0x80) { isAscii = false; break; }
        }
        if (isAscii) {
            cols.uniqueAsciiBits[uid >> 6] |= (1ULL << (uid & 63));
        }
    }

    return cols;
}

} // namespace swiftlist::index_v2
