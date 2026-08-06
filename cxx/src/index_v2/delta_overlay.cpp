#include "index_v2/delta_overlay.h"

#include <cassert>

namespace swiftlist::index_v2 {

DeltaOverlay::DeltaOverlay(int baseRowCount) {
    (void)baseRowCount; // reserved for future capacity hint
}

void DeltaOverlay::Upsert(FileRecordInput record) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. If already in added list, replace it.
    auto addedIt = m_addedById.find(record.Id);
    if (addedIt != m_addedById.end()) {
        DeltaRecord& dr = m_added[addedIt->second];
        dr.Id = record.Id;
        dr.ParentId = record.ParentId;
        dr.Name = std::move(record.Name);
        dr.Flags = record.Flags;
        dr.Size = record.Size;
        dr.CreationTimeUnix = record.CreationTimeUnix;
        dr.LastWriteTimeUnix = record.LastWriteTimeUnix;
        dr.LastAccessTimeUnix = record.LastAccessTimeUnix;
        dr.Removed = false;
        return;
    }

    // 2. Otherwise, add to the added list.
    size_t idx = m_added.size();
    DeltaRecord dr;
    dr.Id = record.Id;
    dr.ParentId = record.ParentId;
    dr.Name = std::move(record.Name);
    dr.Flags = record.Flags;
    dr.Size = record.Size;
    dr.CreationTimeUnix = record.CreationTimeUnix;
    dr.LastWriteTimeUnix = record.LastWriteTimeUnix;
    dr.LastAccessTimeUnix = record.LastAccessTimeUnix;
    dr.Removed = false;
    m_added.push_back(std::move(dr));
    m_addedById[record.Id] = idx;
}

void DeltaOverlay::Remove(UInt128 id, int baseRow) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // If in added list, remove it entirely.
    auto addedIt = m_addedById.find(id);
    if (addedIt != m_addedById.end()) {
        size_t idx = addedIt->second;
        // Swap with last and pop (order doesn't matter for added).
        if (idx + 1 < m_added.size()) {
            m_added[idx] = std::move(m_added.back());
            m_addedById[m_added[idx].Id] = idx;
        }
        m_added.pop_back();
        m_addedById.erase(addedIt);
        return;
    }

    // If base row is known, record the deletion in both m_overrides and m_deleted.
    if (baseRow >= 0) {
        DeltaRecord dr{};
        dr.Id = id;
        dr.Removed = true;
        m_overrides[baseRow] = std::move(dr);
        m_deleted.insert(baseRow);
        return;
    }

    // Base row unknown and id not in added — nothing to do.
}

bool DeltaOverlay::TryLookup(UInt128 id, DeltaRecord& record) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto addedIt = m_addedById.find(id);
    if (addedIt != m_addedById.end()) {
        record = m_added[addedIt->second];
        return true;
    }
    return false;
}

bool DeltaOverlay::Contains(UInt128 id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_addedById.count(id) > 0;
}

void DeltaOverlay::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_added.clear();
    m_addedById.clear();
    m_overrides.clear();
    m_deleted.clear();
}

bool DeltaOverlay::IsEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_added.empty() && m_overrides.empty() && m_deleted.empty();
}

std::vector<DeltaRecord> DeltaOverlay::AddedRecords() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_added;
}

std::unordered_map<int, DeltaRecord> DeltaOverlay::Overrides() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_overrides;
}

std::unordered_set<int> DeltaOverlay::DeletedBaseRows() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_deleted;
}

} // namespace swiftlist::index_v2
