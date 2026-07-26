#include "search/search_matcher.h"

#include "search/fzf_top_n.h"

#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace swiftlist::search {

SearchMatcher::SearchMatcher(const index_v2::Snapshot* snapshot)
    : m_snapshot(snapshot) {}

bool SearchMatcher::PassesFilter(uint32_t uniqueAsciiBits, uint64_t requiredMask) const {
    // If the unique name is ASCII-only, check that its char mask
    // contains all required pattern chars.
    return (uniqueAsciiBits & requiredMask) == requiredMask;
}

std::vector<UniqueMatch> SearchMatcher::SearchTerm(const FzfTerm& term, int maxResults) {
    std::vector<UniqueMatch> results;
    if (!m_snapshot || maxResults <= 0) return results;

    int uniqueCount = m_snapshot->GetMeta().UniqueCount;
    if (uniqueCount == 0) return results;

    uint64_t termMask = 0;
    if (!term.Utf8.empty()) {
        for (unsigned char c : term.Utf8) {
            if (c >= 'a' && c <= 'z') termMask |= (1ULL << (c - 'a'));
            else if (c >= 'A' && c <= 'Z') termMask |= (1ULL << (c - 'A'));
        }
    }

    // Use BoundedTopN for the top results.
    struct ScoredMatch {
        int Score;
        int Uid;
        int Row;
        int Start;
        int End;

        bool operator<(const ScoredMatch& other) const { return Score < other.Score; }
        bool operator>=(const ScoredMatch& other) const { return Score >= other.Score; }
    };

    // Simple growing vector — sort at end for now.
    // (BoundedTopN with heap is a micro-optimization for later.)
    std::vector<ScoredMatch> heap;
    heap.reserve(static_cast<size_t>(maxResults) * 2);

    for (int uid = 0; uid < uniqueCount; ++uid) {
        auto name = m_snapshot->UniqueNameUtf8(uid);

        // Pre-filter: skip non-ASCII names for ASCII terms.
        if (!term.AsciiBytes.empty()) {
            bool isAscii = m_snapshot->IsUniqueAscii(uid);
            if (!isAscii) continue;

            // Char mask filter.
            uint64_t nameMask = 0;
            for (auto b : name) {
                auto c = static_cast<unsigned char>(b);
                if (c >= 'a' && c <= 'z') nameMask |= (1ULL << (c - 'a'));
                else if (c >= 'A' && c <= 'Z') nameMask |= (1ULL << (c - 'A'));
            }
            if ((nameMask & termMask) != termMask) continue;
        }

        // Prepare text buffer.
        m_textBuf.resize(name.size());
        m_bonusBuf.resize(name.size());
        std::memcpy(m_textBuf.data(), name.data(), name.size());

        // Run matcher.
        auto match = FzfMatcher::MatchWithBonuses(m_textBuf, m_bonusBuf, term.AsciiBytes);

        if (match.Matched) {
            // Find a representative row for this UID.
            int repRow = -1;
            const auto& uidRows = m_snapshot->UidRows();
            const auto& uidStarts = m_snapshot->UidStarts();
            int start = uidStarts[uid];
            int end = uidStarts[uid + 1];
            if (start < end) {
                repRow = uidRows[start];
            }

            ScoredMatch sm{match.Score, uid, repRow, match.Start, match.End};

            if (static_cast<int>(heap.size()) < maxResults) {
                heap.push_back(sm);
                if (static_cast<int>(heap.size()) == maxResults) {
                    std::make_heap(heap.begin(), heap.end(), std::greater<>());
                }
            } else if (sm.Score < heap.front().Score) {
                std::pop_heap(heap.begin(), heap.end(), std::greater<>());
                heap.back() = sm;
                std::push_heap(heap.begin(), heap.end(), std::greater<>());
            }
        }
    }

    // Sort ascending by score.
    std::sort(heap.begin(), heap.end(), [](const ScoredMatch& a, const ScoredMatch& b) {
        return a.Score < b.Score;
    });

    results.reserve(heap.size());
    for (auto& sm : heap) {
        results.push_back({sm.Uid, sm.Row, sm.Score, sm.Start, sm.End});
    }
    return results;
}

std::vector<UniqueMatch> SearchMatcher::SearchPattern(const FzfPattern& pattern, int maxResults) {
    if (pattern.Terms.empty()) return {};

    // For simplicity: search each term independently, then intersect.
    // (Full AND semantics with scoring is a refinement for later.)
    std::vector<UniqueMatch> results = SearchTerm(pattern.Terms[0], maxResults);

    // If there are more terms, filter results that don't match subsequent terms.
    for (size_t t = 1; t < pattern.Terms.size(); ++t) {
        auto termResults = SearchTerm(pattern.Terms[t], maxResults);
        // Build a set of UIDs from termResults for fast lookup.
        std::unordered_set<int> termUids;
        for (auto& m : termResults) termUids.insert(m.Uid);

        // Remove results whose UID isn't in termUids.
        results.erase(
            std::remove_if(results.begin(), results.end(),
                           [&](const UniqueMatch& m) { return termUids.count(m.Uid) == 0; }),
            results.end());
    }

    return results;
}

} // namespace swiftlist::search
