#pragma once

#include "search/fzf_scoring.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace swiftlist::search {

enum class TermKind : uint8_t {
    Fuzzy = 0,
    Exact = 1,
    Prefix = 2,
    Suffix = 3,
};

struct FzfTerm {
    TermKind Kind = TermKind::Fuzzy;
    bool Inverse = false; // !term — reject match
    bool CaseSensitive = false;

    // UTF-8 bytes of the pattern (lowercased if !CaseSensitive).
    std::string Utf8;

    // ASCII fast path: only valid when pattern is pure ASCII.
    // If empty, this term cannot use the byte-level fast path.
    std::string AsciiBytes;
};

struct FzfPattern {
    std::vector<FzfTerm> Terms;

    // True if pattern is empty (matches everything).
    [[nodiscard]] bool IsEmpty() const { return Terms.empty(); }

    // Precomputed character mask for all terms (for AVX2 pre-filter).
    [[nodiscard]] uint64_t RequiredMask() const;
};

// Parses a query string into an FzfPattern.
// Handles: term separators (space), prefix (^), suffix ($), exact ('), inverse (!).
FzfPattern ParsePattern(std::string_view query, bool caseSensitive = false);

} // namespace swiftlist::search
