#pragma once

#include "index_v2/snapshot_builder.h"

#include <string>

namespace swiftlist::index_v2 {

class SnapshotWriter {
public:
    // Writes a snapshot to a file atomically via temp-then-rename.
    // The output path must be on the same volume as tempDir (default = output dir).
    static void Write(const std::vector<FileRecordInput>& records,
                      const Meta& metaTemplate,
                      const std::string& path);

private:
    // Writes columnar data to an already-open binary stream (no file ops).
    // Used for tests and for the internal temp-file write.
    static void WriteToStream(const SnapshotColumns& columns, std::ostream& stream);
};

} // namespace swiftlist::index_v2
