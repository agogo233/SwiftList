#pragma once

#include "index_v2/snapshot.h"
#include "index_v2/snapshot_builder.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace swiftlist::index_v2 {

// Delta types for USN-driven modifications.
enum class DeltaAction : uint8_t {
    Add,
    Update,
    Remove,
};

// A single mutable record in the overlay (replaces or adds a row).
struct DeltaRecord {
    UInt128 Id;
    UInt128 ParentId;
    std::string Name;
    FileRecordFlags Flags = FileRecordFlags::None;
    int64_t Size = 0;
    uint32_t CreationTimeUnix = 0;
    uint32_t LastWriteTimeUnix = 0;
    uint32_t LastAccessTimeUnix = 0;
    int ParentBaseRow = -1; // resolved parent base row, -1 = unresolved
    bool Removed = false;
};

// DeltaOverlay: mutable overlay on top of an immutable Snapshot.
// Thread-safe via internal mutex. Readers use Snapshot's const accessors;
// writers lock this overlay and apply modifications.
// Split out from LiveIndex purely to keep each file under the repo's
// per-file line limit; this class has no state of its own beyond the
// overlay maps the owning LiveIndex mutates.
class DeltaOverlay {
public:
    DeltaOverlay() = default;
    ~DeltaOverlay() = default;

    DeltaOverlay(const DeltaOverlay&) = delete;
    DeltaOverlay& operator=(const DeltaOverlay&) = delete;
    DeltaOverlay(DeltaOverlay&&) noexcept = default;
    DeltaOverlay& operator=(DeltaOverlay&&) noexcept = default;

    // Initialize with a known base snapshot row count.
    explicit DeltaOverlay(int baseRowCount);

    // Upsert a record by Id. Thread-safe.
    void Upsert(FileRecordInput record);

    // Remove a record by Id (marks as deleted in overlay or removes from added).
    void Remove(UInt128 id);

    // Lookup: returns true if found. Thread-safe.
    [[nodiscard]] bool TryLookup(UInt128 id, DeltaRecord& record) const;

    // Check if an Id is known to the overlay (added or modified).
    [[nodiscard]] bool Contains(UInt128 id) const;

    // Iterate all added records (not in base snapshot).
    [[nodiscard]] std::vector<DeltaRecord> AddedRecords() const;

    // Iterate overrides to base rows (keyed by base row index).
    [[nodiscard]] std::unordered_map<int, DeltaRecord> Overrides() const;

    // Set of base row indices marked as deleted.
    [[nodiscard]] std::unordered_set<int> DeletedBaseRows() const;

    // Clear all overlay state.
    void Clear();

    // Returns true if the overlay has any pending changes.
    [[nodiscard]] bool IsEmpty() const;

private:
    mutable std::mutex m_mutex;

    // Records added by USN events (not present in base snapshot).
    std::vector<DeltaRecord> m_added;
    // Id -> index in m_added for fast lookup.
    std::unordered_map<UInt128, size_t, std::hash<UInt128>> m_addedById;

    // Base row overrides (keyed by base row index).
    std::unordered_map<int, DeltaRecord> m_overrides;

    // Base row indices marked as deleted.
    std::unordered_set<int> m_deleted;
};

} // namespace swiftlist::index_v2
