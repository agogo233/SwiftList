#include "index_v2/live_index.h"

namespace swiftlist::index_v2 {

LiveIndex::LiveIndex() = default;
LiveIndex::~LiveIndex() = default;

void LiveIndex::OpenSnapshot(const std::string& path) {
    auto snap = std::make_unique<Snapshot>();
    snap->Open(path);
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_snapshot = std::move(snap);
    m_delta.Clear();
}

void LiveIndex::SwapSnapshot(std::unique_ptr<Snapshot> newSnapshot) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_snapshot = std::move(newSnapshot);
    m_delta.Clear();
}

void LiveIndex::Mutate(std::function<void(Snapshot&, DeltaOverlay&)> mutator) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_snapshot) {
        mutator(*m_snapshot, m_delta);
    }
}

int LiveIndex::RowCount() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_snapshot ? m_snapshot->RowCount() : 0;
}

} // namespace swiftlist::index_v2
