#pragma once

#include <array>
#include <cstdint>

namespace swiftlist::search {

// --- Scoring constants (must match C# FzfAlgorithm exactly) ---

inline constexpr int ScoreMatch = 16;
inline constexpr int ScoreGapStart = -3;
inline constexpr int ScoreGapExtension = -1;
inline constexpr int BonusBoundary = 8;         // ScoreMatch / 2
inline constexpr int BonusNonWord = 8;
inline constexpr int BonusCamel123 = 7;         // BonusBoundary + ScoreGapExtension
inline constexpr int BonusConsecutive = 4;      // -(ScoreGapStart + ScoreGapExtension)
inline constexpr int BonusFirstCharMultiplier = 2;
inline constexpr int BonusBoundaryWhite = 10;   // BonusBoundary + 2
inline constexpr int BonusBoundaryDelimiter = 9; // BonusBoundary + 1
inline constexpr int MaxV2Cells = 250'000;

// --- Character classes ---

enum class CharClass : uint8_t {
    White = 0,
    NonWord = 1,
    Delimiter = 2,
    Lower = 3,
    Upper = 4,
    Letter = 5,
    Number = 6,
};

// Classify a single ASCII byte.
CharClass ClassifyAscii(uint8_t c);

// --- Bonus table ---

// Indexed as [scheme][prevClass << 3 | currClass]
// 7 classes * 8 prev = 56 entries per scheme.
using BonusTable = std::array<std::array<int8_t, 56>, 2>;

// Returns the static bonus table.
const BonusTable& GetBonusTable();

// Look up the bonus for a (prevClass, currClass) pair (Default scheme).
inline int GetBonus(CharClass prev, CharClass curr) {
    return GetBonusTable()[0][(static_cast<int>(prev) << 3) | static_cast<int>(curr)];
}

} // namespace swiftlist::search
