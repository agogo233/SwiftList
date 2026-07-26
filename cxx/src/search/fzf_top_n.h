#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

namespace swiftlist::search {

// Bounded max-heap of sort keys. Keeps the N smallest entries.
// When full, a new entry replaces the current worst (largest) only if it is smaller.
// Scalar implementation (no SIMD) — sufficient for correctness; AVX2 FindWorstIndex
// is a micro-optimization that can be added later.
template <typename T, int Capacity>
class BoundedTopN {
public:
    BoundedTopN() = default;

    void Reset() { m_count = 0; }

    // Add a value. If full and value >= current worst, discard.
    void Add(const T& value) {
        if (m_count < Capacity) {
            m_entries[m_count] = value;
            ++m_count;
            if (m_count == Capacity) {
                // Find worst (largest).
                m_worstIndex = static_cast<int>(
                    std::max_element(m_entries.begin(), m_entries.begin() + m_count) - m_entries.begin());
            }
            return;
        }
        // Full: only add if better than worst.
        if (value >= m_entries[m_worstIndex]) return;
        m_entries[m_worstIndex] = value;
        // Recompute worst.
        m_worstIndex = static_cast<int>(
            std::max_element(m_entries.begin(), m_entries.begin() + m_count) - m_entries.begin());
    }

    // Remove and return the best (smallest) element.
    T PopBest() {
        assert(m_count > 0);
        int bestIdx = static_cast<int>(
            std::min_element(m_entries.begin(), m_entries.begin() + m_count) - m_entries.begin());
        T result = m_entries[bestIdx];
        // Swap with last and decrement.
        m_entries[bestIdx] = m_entries[m_count - 1];
        --m_count;
        m_worstIndex = -1; // will be recomputed lazily
        return result;
    }

    [[nodiscard]] int Count() const { return m_count; }
    [[nodiscard]] bool IsEmpty() const { return m_count == 0; }
    [[nodiscard]] bool IsFull() const { return m_count == Capacity; }

    // Access entries (unsorted).
    [[nodiscard]] const T* Data() const { return m_entries.data(); }

    // Returns the current worst (largest) entry value, or UINT64_MAX if empty.
    [[nodiscard]] T WorstValue() const {
        if (m_count == 0) return std::numeric_limits<T>::max();
        if (m_worstIndex < 0 || m_worstIndex >= m_count) {
            m_worstIndex = static_cast<int>(
                std::max_element(m_entries.begin(), m_entries.begin() + m_count) - m_entries.begin());
        }
        return m_entries[m_worstIndex];
    }

    // Sort remaining entries ascending (best first).
    void SortAscending() {
        std::sort(m_entries.begin(), m_entries.begin() + m_count);
    }

private:
    std::array<T, Capacity> m_entries;
    int m_count = 0;
    mutable int m_worstIndex = -1;
};

} // namespace swiftlist::search
