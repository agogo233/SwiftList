#include "search/fzf_matcher.h"

#include <algorithm>
#include <cstring>

namespace swiftlist::search {

// --- Bonus computation ---

void FzfMatcher::ComputeBonuses(std::span<const uint8_t> chars, std::span<int8_t> bonuses) {
    if (chars.empty()) return;

    CharClass prev = CharClass::White; // implicit start-of-string is White boundary
    for (size_t i = 0; i < chars.size(); ++i) {
        CharClass cur = ClassifyAscii(chars[i]);
        bonuses[i] = static_cast<int8_t>(GetBonus(prev, cur));
        prev = cur;
    }
}

// --- V2 DP matching ---

FzfMatchResult FzfMatcher::MatchV2(std::span<const uint8_t> chars,
                                    std::span<int8_t> bonuses,
                                    std::string_view pattern) {
    FzfMatchResult result;
    int n = static_cast<int>(chars.size());
    int m = static_cast<int>(pattern.size());

    if (m == 0 || n == 0 || m > n) return result;

    // Find scope: first occurrence of each pattern character.
    int firstIdx = n; // earliest pos where pattern[0] matches
    for (int i = 0; i < n; ++i) {
        if (chars[i] == static_cast<uint8_t>(pattern[0])) {
            firstIdx = i;
            break;
        }
    }
    if (firstIdx >= n) return result; // pattern[0] not found

    int lastIdx = -1; // latest pos where pattern[m-1] matches
    for (int i = n - 1; i >= 0; --i) {
        if (chars[i] == static_cast<uint8_t>(pattern[m - 1])) {
            lastIdx = i;
            break;
        }
    }
    if (lastIdx < 0) return result;

    int width = lastIdx - firstIdx + 1;
    if (m * width > MaxV2Cells) {
        return MatchV1(chars, bonuses, pattern);
    }

    // Allocate matrices.
    std::vector<int> scores(static_cast<size_t>(m) * width);
    std::vector<int> consecutive(static_cast<size_t>(m) * width);

    // Initialize first row (pattern[0]).
    {
        int s0 = static_cast<int>(pattern[0]);
        for (int col = firstIdx; col <= lastIdx; ++col) {
            int rel = col - firstIdx;
            if (chars[col] == s0) {
                int sc = ScoreMatch + bonuses[col] * BonusFirstCharMultiplier;
                scores[rel] = sc;
                consecutive[rel] = 1;
            } else {
                int prev = (rel > 0) ? scores[rel - 1] : 0;
                int gap = ScoreGapStart; // first gap
                int sc = std::max(prev + gap, 0);
                scores[rel] = sc;
                consecutive[rel] = 0;
            }
        }
    }

    // Iterate pattern[1..m-1].
    int maxScore = 0;
    int maxScorePos = -1;
    for (int pidx = 1; pidx < m; ++pidx) {
        int row = pidx * width;
        int prevRow = row - width;
        int patChar = static_cast<int>(pattern[pidx]);

        bool inGap = false;
        for (int col = firstIdx; col <= lastIdx; ++col) {
            int rel = col - firstIdx;

            // s2: gap (left neighbor).
            int s2 = (rel > 0) ? scores[row + rel - 1] + (inGap ? ScoreGapExtension : ScoreGapStart) : 0;

            // s1: match (diagonal).
            int s1 = 0;
            int consScore = 0;
            if (rel > 0 && chars[col] == patChar) {
                s1 = scores[prevRow + rel - 1] + ScoreMatch;
                consScore = consecutive[prevRow + rel - 1] + 1;

                // Consecutive bonus logic.
                if (consScore > 1) {
                    int firstBonus = bonuses[col - consScore + 1];
                    if (bonuses[col] >= BonusBoundary && bonuses[col] > firstBonus) {
                        consScore = 1;
                    } else {
                        bonuses[col] = std::max<int8_t>(
                                                  std::max<int8_t>(firstBonus,
                                                           static_cast<int8_t>(BonusConsecutive)),
                                                  bonuses[col]);
                    }
                }

                if (s1 + bonuses[col] < s2) {
                    s1 += bonuses[col];
                    consScore = 0;
                } else {
                    s1 += bonuses[col];
                }
            } else {
                s1 = 0;
                consScore = 0;
            }

            int cellScore = std::max({s1, s2, 0});
            scores[row + rel] = cellScore;
            consecutive[row + rel] = consScore;
            inGap = (s1 < s2);

            // Track max in last row.
            if (pidx == m - 1 && cellScore > maxScore) {
                maxScore = cellScore;
                maxScorePos = col;
            }
        }
    }

    if (maxScore <= 0) return result;

    // Backtrack to find start position.
    int i = m - 1;
    int j = maxScorePos;
    while (i >= 0 && j >= firstIdx) {
        int rel = j - firstIdx;
        int score = scores[i * width + rel];

        if (i == 0) {
            result.Start = j;
            result.End = maxScorePos + 1;
            result.Score = maxScore;
            result.Matched = true;
            return result;
        }

        int diagonal = (rel > 0) ? scores[(i - 1) * width + rel - 1] : -1;
        int left = (rel > 0) ? scores[i * width + rel - 1] : -1;
        bool preferMatch = (consecutive[i * width + rel] > 1);

        if (score > diagonal && (score > left || (score == left && preferMatch))) {
            --i;
        }
        --j;
    }

    // Fallback if backtrack fails.
    result.Start = firstIdx;
    result.End = maxScorePos + 1;
    result.Score = maxScore;
    result.Matched = true;
    return result;
}

// --- V1 greedy matching (fallback) ---

FzfMatchResult FzfMatcher::MatchV1(std::span<const uint8_t> chars,
                                    std::span<int8_t> bonuses,
                                    std::string_view pattern) {
    FzfMatchResult result;
    int n = static_cast<int>(chars.size());
    int m = static_cast<int>(pattern.size());

    if (m == 0 || n == 0 || m > n) return result;

    // Greedy: find earliest occurrence of each pattern char in order.
    int patIdx = 0;
    int matchStart = -1;
    int matchEnd = -1;
    int score = 0;
    bool inGap = false;
    int consecutive = 0;
    CharClass prevClass = CharClass::White;
    int firstBonus = 0;

    for (int i = 0; i < n && patIdx < m; ++i) {
        if (chars[i] == static_cast<uint8_t>(pattern[patIdx])) {
            if (matchStart < 0) matchStart = i;

            CharClass cur = ClassifyAscii(chars[i]);
            int bonus = GetBonus(prevClass, cur);

            if (consecutive == 0) {
                firstBonus = bonus;
            } else {
                if (bonus >= BonusBoundary && bonus > firstBonus) {
                    firstBonus = bonus;
                }
                bonus = std::max(bonus,
                                    std::max(firstBonus, BonusConsecutive));
            }

            if (patIdx == 0) {
                score += ScoreMatch + bonus * BonusFirstCharMultiplier;
            } else {
                score += ScoreMatch + bonus;
            }

            matchEnd = i + 1;
            ++consecutive;
            inGap = false;
            prevClass = cur;
            ++patIdx;
        } else {
            score += inGap ? ScoreGapExtension : ScoreGapStart;
            inGap = true;
            consecutive = 0;
            prevClass = ClassifyAscii(chars[i]);
        }
    }

    if (patIdx < m) return result; // not all pattern chars matched

    result.Matched = true;
    result.Start = matchStart;
    result.End = matchEnd;
    result.Score = std::max(score, 0);
    return result;
}

// --- Public Match ---

FzfMatchResult FzfMatcher::Match(std::string_view text, std::string_view pattern, bool caseSensitive) {
    FzfMatchResult result;
    if (pattern.empty()) {
        result.Matched = true;
        result.Start = 0;
        result.End = 0;
        result.Score = 0;
        return result;
    }
    if (text.empty() || pattern.size() > text.size()) return result;

    // Prepare buffers.
    std::vector<uint8_t> chars(text.size());
    std::vector<int8_t> bonuses(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        chars[i] = static_cast<uint8_t>(text[i]);
        if (!caseSensitive && text[i] >= 'A' && text[i] <= 'Z')
            chars[i] = static_cast<uint8_t>(text[i] - 'A' + 'a');
    }

    ComputeBonuses(chars, bonuses);

    int cells = static_cast<int>(pattern.size()) * static_cast<int>(text.size());
    if (cells <= MaxV2Cells && pattern.size() <= 1000) {
        return MatchV2(chars, bonuses, pattern);
    }
    return MatchV1(chars, bonuses, pattern);
}

FzfMatchResult FzfMatcher::MatchWithBonuses(std::span<const uint8_t> chars,
                                              std::span<int8_t> bonuses,
                                             std::string_view pattern) {
    if (pattern.empty()) {
        return {true, 0, 0, 0};
    }
    if (chars.empty() || pattern.size() > chars.size()) return {};

    int cells = static_cast<int>(pattern.size()) * static_cast<int>(chars.size());
    if (cells <= MaxV2Cells && pattern.size() <= 1000) {
        return MatchV2(chars, bonuses, pattern);
    }
    return MatchV1(chars, bonuses, pattern);
}

} // namespace swiftlist::search
