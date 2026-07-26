#pragma once

#include "search/fzf_pattern.h"
#include "search/fzf_matcher.h"
#include "index_v2/snapshot.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace swiftlist::search {

struct UniqueMatch {
    int Uid;       // unique name ID
    int Row;       // representative row index
    int Score;
    int Start;     // match start byte offset within the name
    int End;       // match end byte offset
};

// Searches a Snapshot's unique names using the Fzf fuzzy matching algorithm.
// This is the single-threaded core; parallel dispatch is done by SearchCoordinator
// (deferred to a later phase when we have the USN/live index machinery).
class SearchMatcher {
public:
    explicit SearchMatcher(const index_v2::Snapshot* snapshot);

    // Runs a search for a single term against all unique names.
    // Returns matches sorted by score (best first), up to maxResults.
    std::vector<UniqueMatch> SearchTerm(const FzfTerm& term, int maxResults = 100);

    // Runs a full pattern search (multiple terms AND-ed together).
    std::vector<UniqueMatch> SearchPattern(const FzfPattern& pattern, int maxResults = 100);

    // Pre-filter: skip unique names whose char mask doesn't contain all required chars.
    [[nodiscard]] bool PassesFilter(uint32_t uniqueAsciiBits, uint64_t requiredMask) const;

private:
    const index_v2::Snapshot* m_snapshot;
    std::vector<uint8_t> m_textBuf;     // reusable buffer for text normalization
    std::vector<int8_t> m_bonusBuf;     // reusable buffer for bonuses
};

} // namespace swiftlist::search
