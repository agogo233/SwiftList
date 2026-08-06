#include "search/fzf_scoring.h"

#include <algorithm>

namespace swiftlist::search {

CharClass ClassifyAscii(uint8_t c) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return CharClass::White;
    if (c >= 'a' && c <= 'z') return CharClass::Lower;
    if (c >= 'A' && c <= 'Z') return CharClass::Upper;
    if (c >= '0' && c <= '9') return CharClass::Number;
    // Delimiters: / \ : ; , |
    if (c == '/' || c == '\\' || c == ':' || c == ';' || c == ',' || c == '|')
        return CharClass::Delimiter;
    // Non-word: everything else (punctuation, symbols)
    return CharClass::NonWord;
}

namespace {

// Build the default bonus table matching C# FzfAlgorithm.Bonuses.
BonusTable BuildBonusTable() {
    BonusTable table{};

    for (int scheme = 0; scheme < 2; ++scheme) {
        for (int prev = 0; prev < 7; ++prev) {
            for (int curr = 0; curr < 8; ++curr) {
                int8_t bonus = 0;
                auto cur = static_cast<CharClass>(curr);

                if (cur <= CharClass::Delimiter) {
                    // White, NonWord, Delimiter
                    if (prev == static_cast<int>(CharClass::White))
                        bonus = BonusBoundaryWhite;
                    else if (prev == static_cast<int>(CharClass::Delimiter))
                        bonus = BonusBoundaryDelimiter;
                    else if (prev == static_cast<int>(CharClass::NonWord))
                        bonus = BonusBoundary;
                } else if (cur == CharClass::Lower || cur == CharClass::Upper || cur == CharClass::Number || cur == CharClass::Letter) {
                    // A word-class character
                    if (prev == static_cast<int>(CharClass::Lower)) {
                        bonus = 0;
                    } else if (prev == static_cast<int>(CharClass::Upper)) {
                        // Lower-to-Upper: camelCase boundary
                        if (cur == CharClass::Lower)
                            bonus = BonusCamel123;
                        else
                            bonus = 0;
                    } else if (prev == static_cast<int>(CharClass::Number)) {
                        if (cur != CharClass::Number)
                            bonus = BonusCamel123;
                    } else if (prev == static_cast<int>(CharClass::NonWord)) {
                        bonus = BonusNonWord;
                    } else if (prev == static_cast<int>(CharClass::Delimiter)) {
                        bonus = BonusBoundaryDelimiter;
                    } else if (prev == static_cast<int>(CharClass::White)) {
                        bonus = BonusBoundaryWhite;
                    }
                }

                // Path scheme adds extra bonuses (simplified: same as default for now)
                if (scheme == 1 && bonus == 0 && cur >= CharClass::Lower && cur <= CharClass::Number) {
                    if (prev == static_cast<int>(CharClass::Delimiter))
                        bonus = BonusBoundaryDelimiter;
                }

                table[scheme][(prev << 3) | curr] = bonus;
            }
        }
    }
    return table;
}

} // namespace

const BonusTable& GetBonusTable() {
    static const BonusTable table = BuildBonusTable();
    return table;
}

} // namespace swiftlist::search
