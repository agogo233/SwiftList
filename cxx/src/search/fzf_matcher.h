#pragma once

#include "search/fzf_pattern.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace swiftlist::search {

struct FzfMatchResult {
    bool Matched = false;
    int Start = -1;  // byte offset in text where match begins
    int End = -1;    // byte offset in text where match ends (exclusive)
    int Score = 0;
};

class FzfMatcher {
public:
    // Performs byte-level fuzzy matching.
    // text and pattern should already be normalized (lowercased if case-insensitive).
    [[nodiscard]] static FzfMatchResult Match(std::string_view text,
                                               std::string_view pattern,
                                               bool caseSensitive = false);

// Byte-level matching with pre-classified character bonuses.
    // chars: normalized bytes of text
    // bonuses: per-position bonus values (must be same length as chars)
    [[nodiscard]] static FzfMatchResult MatchWithBonuses(std::span<const uint8_t> chars,
                                                         std::span<int8_t> bonuses,
                                                         std::string_view pattern);

    // Compute per-position bonuses for a byte span.
    static void ComputeBonuses(std::span<const uint8_t> chars, std::span<int8_t> bonuses);

private:
    // V2 DP algorithm. Returns match result.
[[nodiscard]] static FzfMatchResult MatchV2(std::span<const uint8_t> chars,
                                                  std::span<int8_t> bonuses,
                                                 std::string_view pattern);

    // V1 greedy fallback for very long patterns.
[[nodiscard]] static FzfMatchResult MatchV1(std::span<const uint8_t> chars,
                                                   std::span<int8_t> bonuses,
                                                  std::string_view pattern);
};

} // namespace swiftlist::search
