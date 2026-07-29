#pragma once

#include "indexer/file_record_store.h"
#include "indexer/mft_parser.h"
#include "indexer/volume_helper.h"

#include <functional>

namespace swiftlist::indexer {

using index_v2::FileRecordInput;

class MftIndexScanner {
public:
    using Callback = std::function<void(const FileRecordInput&)>;

    bool Scan(wchar_t driveLetter, FileRecordStore& store, Callback cb = nullptr);
};

} // namespace swiftlist::indexer
