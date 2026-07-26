#pragma once

#include "index_v2/snapshot.h"
#include "index_v2/delta_overlay.h"

#include <functional>
#include <memory>
#include <shared_mutex>

namespace swiftlist::index_v2 {

// LiveIndex: an immutable Snapshot plus a mutable DeltaOverlay,
// guarded by a shared_mutex for concurrent reads and exclusive writes.
class LiveIndex {
public:
    LiveIndex();
    ~LiveIndex();

    // Non-copyable.
    LiveIndex(const LiveIndex&) = delete;
    LiveIndex& operator=(const LiveIndex&) = delete;

    // Opens a snapshot from disk and initializes the overlay.
    void OpenSnapshot(const std::string& path);

    // Swaps in a new snapshot (rebuilds overlay). Thread-safe.
    void SwapSnapshot(std::unique_ptr<Snapshot> newSnapshot);

    // Read operation: runs under shared (read) lock.
    template <typename T>
    T Read(std::function<T(const Snapshot&, const DeltaOverlay&)> reader) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return reader(*m_snapshot, m_delta);
    }

    // Write operation: runs under exclusive (write) lock.
    void Mutate(std::function<void(Snapshot&, DeltaOverlay&)> mutator);

    // Returns true if a snapshot is loaded.
    [[nodiscard]] bool IsLoaded() const { return m_snapshot != nullptr; }

    // Snapshot metadata (read lock).
    [[nodiscard]] int RowCount() const;

private:
    mutable std::shared_mutex m_mutex;
    std::unique_ptr<Snapshot> m_snapshot;
    DeltaOverlay m_delta;
};

} // namespace swiftlist::index_v2
