#include "search/fzf_scoring.h"
#include "search/fzf_pattern.h"
#include "search/fzf_matcher.h"
#include "search/fzf_top_n.h"

#include <gtest/gtest.h>

using namespace swiftlist::search;

// --- Scoring constants ---

TEST(FzfScoringTest, ConstantsMatchCs) {
    EXPECT_EQ(ScoreMatch, 16);
    EXPECT_EQ(ScoreGapStart, -3);
    EXPECT_EQ(ScoreGapExtension, -1);
    EXPECT_EQ(BonusBoundary, 8);
    EXPECT_EQ(BonusNonWord, 8);
    EXPECT_EQ(BonusCamel123, 7);
    EXPECT_EQ(BonusConsecutive, 4);
    EXPECT_EQ(BonusFirstCharMultiplier, 2);
    EXPECT_EQ(BonusBoundaryWhite, 10);
    EXPECT_EQ(BonusBoundaryDelimiter, 9);
    EXPECT_EQ(MaxV2Cells, 250'000);
}

TEST(FzfScoringTest, ClassifyAscii) {
    EXPECT_EQ(ClassifyAscii('a'), CharClass::Lower);
    EXPECT_EQ(ClassifyAscii('z'), CharClass::Lower);
    EXPECT_EQ(ClassifyAscii('A'), CharClass::Upper);
    EXPECT_EQ(ClassifyAscii('Z'), CharClass::Upper);
    EXPECT_EQ(ClassifyAscii('0'), CharClass::Number);
    EXPECT_EQ(ClassifyAscii('9'), CharClass::Number);
    EXPECT_EQ(ClassifyAscii(' '), CharClass::White);
    EXPECT_EQ(ClassifyAscii('\t'), CharClass::White);
    EXPECT_EQ(ClassifyAscii('/'), CharClass::Delimiter);
    EXPECT_EQ(ClassifyAscii('\\'), CharClass::Delimiter);
    EXPECT_EQ(ClassifyAscii(':'), CharClass::Delimiter);
    EXPECT_EQ(ClassifyAscii(';'), CharClass::Delimiter);
    EXPECT_EQ(ClassifyAscii(','), CharClass::Delimiter);
    EXPECT_EQ(ClassifyAscii('|'), CharClass::Delimiter);
    EXPECT_EQ(ClassifyAscii('!'), CharClass::NonWord);
    EXPECT_EQ(ClassifyAscii('@'), CharClass::NonWord);
    EXPECT_EQ(ClassifyAscii('#'), CharClass::NonWord);
    EXPECT_EQ(ClassifyAscii('_'), CharClass::NonWord);
}

TEST(FzfScoringTest, BonusTable) {
    // White -> Lower should give BonusBoundaryWhite.
    EXPECT_EQ(GetBonus(CharClass::White, CharClass::Lower), BonusBoundaryWhite);
    // Delimiter -> Lower should give BonusBoundaryDelimiter.
    EXPECT_EQ(GetBonus(CharClass::Delimiter, CharClass::Lower), BonusBoundaryDelimiter);
    // Lower -> Lower should give 0.
    EXPECT_EQ(GetBonus(CharClass::Lower, CharClass::Lower), 0);
    // Upper -> Lower (camelCase) should give BonusCamel123.
    EXPECT_EQ(GetBonus(CharClass::Upper, CharClass::Lower), BonusCamel123);
}

// --- Pattern parsing ---

TEST(FzfPatternTest, EmptyQuery) {
    auto p = ParsePattern("");
    EXPECT_TRUE(p.Terms.empty());
    EXPECT_TRUE(p.IsEmpty());
}

TEST(FzfPatternTest, SingleFuzzyTerm) {
    auto p = ParsePattern("abc");
    ASSERT_EQ(p.Terms.size(), 1);
    EXPECT_EQ(p.Terms[0].Kind, TermKind::Fuzzy);
    EXPECT_EQ(p.Terms[0].Utf8, "abc");
    EXPECT_EQ(p.Terms[0].AsciiBytes, "abc");
    EXPECT_FALSE(p.Terms[0].Inverse);
    EXPECT_FALSE(p.Terms[0].CaseSensitive);
}

TEST(FzfPatternTest, MultipleTerms) {
    auto p = ParsePattern("read md");
    ASSERT_EQ(p.Terms.size(), 2);
    EXPECT_EQ(p.Terms[0].Utf8, "read");
    EXPECT_EQ(p.Terms[1].Utf8, "md");
}

TEST(FzfPatternTest, PrefixAnchor) {
    auto p = ParsePattern("^abc");
    ASSERT_EQ(p.Terms.size(), 1);
    EXPECT_EQ(p.Terms[0].Kind, TermKind::Prefix);
    EXPECT_EQ(p.Terms[0].Utf8, "abc");
}

TEST(FzfPatternTest, SuffixAnchor) {
    auto p = ParsePattern("abc$");
    ASSERT_EQ(p.Terms.size(), 1);
    EXPECT_EQ(p.Terms[0].Kind, TermKind::Suffix);
    EXPECT_EQ(p.Terms[0].Utf8, "abc");
}

TEST(FzfPatternTest, ExactMatch) {
    auto p = ParsePattern("^abc$");
    ASSERT_EQ(p.Terms.size(), 1);
    EXPECT_EQ(p.Terms[0].Kind, TermKind::Exact);
    EXPECT_EQ(p.Terms[0].Utf8, "abc");
}

TEST(FzfPatternTest, ExactQuote) {
    auto p = ParsePattern("'abc");
    ASSERT_EQ(p.Terms.size(), 1);
    EXPECT_EQ(p.Terms[0].Kind, TermKind::Exact);
    EXPECT_EQ(p.Terms[0].Utf8, "abc");
}

TEST(FzfPatternTest, Inverse) {
    auto p = ParsePattern("!abc");
    ASSERT_EQ(p.Terms.size(), 1);
    EXPECT_TRUE(p.Terms[0].Inverse);
    EXPECT_EQ(p.Terms[0].Utf8, "abc");
}

TEST(FzfPatternTest, CaseInsensitiveLowercases) {
    auto p = ParsePattern("ABC", false);
    ASSERT_EQ(p.Terms.size(), 1);
    EXPECT_EQ(p.Terms[0].Utf8, "abc"); // lowercased
}

TEST(FzfPatternTest, CaseSensitivePreserves) {
    auto p = ParsePattern("ABC", true);
    ASSERT_EQ(p.Terms.size(), 1);
    EXPECT_EQ(p.Terms[0].Utf8, "ABC"); // preserved
}

TEST(FzfPatternTest, RequiredMask) {
    auto p = ParsePattern("abc");
    uint64_t mask = p.RequiredMask();
    EXPECT_TRUE(mask & (1ULL << ('a' - 'a')));
    EXPECT_TRUE(mask & (1ULL << ('b' - 'a')));
    EXPECT_TRUE(mask & (1ULL << ('c' - 'a')));
    EXPECT_FALSE(mask & (1ULL << ('z' - 'a')));
}

// --- FzfMatcher ---

TEST(FzfMatcherTest, EmptyPatternMatchesEverything) {
    auto result = FzfMatcher::Match("anything", "");
    EXPECT_TRUE(result.Matched);
    EXPECT_EQ(result.Start, 0);
    EXPECT_EQ(result.End, 0);
    EXPECT_EQ(result.Score, 0);
}

TEST(FzfMatcherTest, PatternLongerThanText) {
    auto result = FzfMatcher::Match("abc", "abcdef");
    EXPECT_FALSE(result.Matched);
}

TEST(FzfMatcherTest, ExactSubstring) {
    auto result = FzfMatcher::Match("readme.md", "read");
    EXPECT_TRUE(result.Matched);
    EXPECT_EQ(result.Start, 0);
    EXPECT_EQ(result.End, 4);
    EXPECT_GT(result.Score, 0);
}

TEST(FzfMatcherTest, FuzzyMatchScattered) {
    auto result = FzfMatcher::Match("swiftlist", "swl");
    EXPECT_TRUE(result.Matched);
    // s=0, w=2, l=6
    EXPECT_EQ(result.Start, 0);
    EXPECT_EQ(result.End, 7); // l is at index 6, end is 7
}

TEST(FzfMatcherTest, NoMatchDisordered) {
    auto result = FzfMatcher::Match("read", "dare");
    EXPECT_FALSE(result.Matched);
}

TEST(FzfMatcherTest, CaseInsensitiveMatch) {
    auto result = FzfMatcher::Match("README.md", "read", false);
    EXPECT_TRUE(result.Matched);
    EXPECT_EQ(result.Start, 0);
    EXPECT_EQ(result.End, 4);
}

TEST(FzfMatcherTest, CaseSensitiveNoMatch) {
    auto result = FzfMatcher::Match("README.md", "read", true);
    EXPECT_FALSE(result.Matched);
}

TEST(FzfMatcherTest, ConsecutiveHigherThanScattered) {
    auto consecutive = FzfMatcher::Match("abcdef", "abc");
    auto scattered = FzfMatcher::Match("aXbYcdef", "abc");
    EXPECT_TRUE(consecutive.Matched);
    EXPECT_TRUE(scattered.Matched);
    EXPECT_GT(consecutive.Score, scattered.Score);
}

TEST(FzfMatcherTest, FirstCharBonus) {
    // Match at start of string should score higher than match in middle.
    auto atStart = FzfMatcher::Match("abcdef", "abc");
    auto inMiddle = FzfMatcher::Match("xyzabc", "abc");
    EXPECT_TRUE(atStart.Matched);
    EXPECT_TRUE(inMiddle.Matched);
    EXPECT_GT(atStart.Score, inMiddle.Score);
}

TEST(FzfMatcherTest, BoundaryBonus) {
    // "my.read.txt" should match "read" with a boundary bonus.
    auto withBoundary = FzfMatcher::Match("my.read.txt", "read");
    auto withoutBoundary = FzfMatcher::Match("myreadtxt", "read");
    EXPECT_TRUE(withBoundary.Matched);
    EXPECT_TRUE(withoutBoundary.Matched);
    EXPECT_GT(withBoundary.Score, withoutBoundary.Score);
}

TEST(FzfMatcherTest, EmptyText) {
    auto result = FzfMatcher::Match("", "abc");
    EXPECT_FALSE(result.Matched);
}

// --- BoundedTopN ---

TEST(BoundedTest, Empty) {
    BoundedTopN<int, 4> topn;
    EXPECT_TRUE(topn.IsEmpty());
    EXPECT_EQ(topn.Count(), 0);
}

TEST(BoundedTest, AddBelowCapacity) {
    BoundedTopN<int, 4> topn;
    topn.Add(5);
    topn.Add(3);
    topn.Add(7);
    EXPECT_EQ(topn.Count(), 3);
    EXPECT_FALSE(topn.IsFull());
}

TEST(BoundedTest, AddAboveCapacityDiscards) {
    BoundedTopN<int, 3> topn;
    topn.Add(5);
    topn.Add(3);
    topn.Add(7);
    topn.Add(10); // worse than 7, discarded
    EXPECT_EQ(topn.Count(), 3);
    EXPECT_TRUE(topn.IsFull());
}

TEST(BoundedTest, AddBetterThanWorstReplaces) {
    BoundedTopN<int, 3> topn;
    topn.Add(5);
    topn.Add(3);
    topn.Add(7); // now full, worst = 7
    topn.Add(1); // better than 7
    EXPECT_EQ(topn.Count(), 3);
    // Entries should be {5, 3, 1}
    topn.SortAscending();
    EXPECT_EQ(topn.Data()[0], 1);
    EXPECT_EQ(topn.Data()[1], 3);
    EXPECT_EQ(topn.Data()[2], 5);
}

TEST(BoundedTest, PopBestOrder) {
    BoundedTopN<int, 5> topn;
    topn.Add(5);
    topn.Add(3);
    topn.Add(7);
    topn.Add(1);
    EXPECT_EQ(topn.PopBest(), 1);
    EXPECT_EQ(topn.PopBest(), 3);
    EXPECT_EQ(topn.PopBest(), 5);
    EXPECT_EQ(topn.PopBest(), 7);
    EXPECT_TRUE(topn.IsEmpty());
}

// --- V2 scoring correctness ---
// Golden values hand-computed against C# FzfAlgorithm constants.

TEST(FzfMatcherTest, ScoringExactMatch) {
    // "abc" matching "abc" exactly: boundary bonus + 3*ScoreMatch + consecutive bonus for each char
    auto result = FzfMatcher::Match("abc", "abc");
    EXPECT_TRUE(result.Matched);
    EXPECT_EQ(result.Score, 88);
}

TEST(FzfMatcherTest, ScoringFuzzySkipChar) {
    // "abc" matching "ac" (skip 'b'): lower score than exact
    auto result = FzfMatcher::Match("abc", "ac");
    EXPECT_TRUE(result.Matched);
    EXPECT_EQ(result.Score, 49);
}

TEST(FzfMatcherTest, ScoringHelloFuzzy) {
    // "hello" matching "hlo" (skip 'e', 'lo' at end is consecutive)
    auto result = FzfMatcher::Match("hello", "hlo");
    EXPECT_TRUE(result.Matched);
    EXPECT_EQ(result.Score, 68);
}

TEST(FzfMatcherTest, ScoringCaseInsensitiveUppercasePattern) {
    // "abc" matching "ABC" case-insensitive should equal "abc" vs "abc"
    auto result = FzfMatcher::Match("abc", "ABC", false);
    EXPECT_TRUE(result.Matched);
    EXPECT_EQ(result.Score, 88);
}

TEST(FzfMatcherTest, ScoringBoundaryDelimiter) {
    // "my.read.txt" matching "read": '.' is delimiter → boundary bonus
    auto result = FzfMatcher::Match("my.read.txt", "read");
    EXPECT_TRUE(result.Matched);
    // Boundary bonus (Delimiter->Lower = 9) should be applied at 'r' position
    EXPECT_GT(result.Score, 0);
}

TEST(FzfMatcherTest, ScoringConsecutiveVsScattered) {
    // "abcdef" vs "def" (consecutive end) should score higher than "axbycdef" vs "def" (scattered)
    auto consecutive = FzfMatcher::Match("abcdef", "def");
    auto scattered = FzfMatcher::Match("axbycdef", "def");
    EXPECT_TRUE(consecutive.Matched);
    EXPECT_TRUE(scattered.Matched);
    EXPECT_GT(consecutive.Score, scattered.Score);
}

TEST(FzfMatcherTest, ScoringFirstCharBonus) {
    // Match at start gets White→Lower bonus (10), match in middle gets Lower→Lower (0)
    // Both with first char multiplier (2x)
    auto atStart = FzfMatcher::Match("file.txt", "file");
    auto inMiddle = FzfMatcher::Match("prefix_file.txt", "file");
    EXPECT_TRUE(atStart.Matched);
    EXPECT_TRUE(inMiddle.Matched);
    EXPECT_GT(atStart.Score, inMiddle.Score);
}

TEST(FzfMatcherTest, MatchWithBonusesPopulated) {
    // Verify that the MatchWithBonuses path (used by SearchMatcher) works correctly
    // when bonuses are pre-computed.
    std::string text = "hello";
    std::vector<uint8_t> chars(text.begin(), text.end());
    std::vector<int8_t> bonuses(chars.size());
    FzfMatcher::ComputeBonuses(chars, bonuses);
    auto result = FzfMatcher::MatchWithBonuses(chars, bonuses, "hlo");
    EXPECT_TRUE(result.Matched);
    EXPECT_EQ(result.Score, 68);
}

TEST(FzfMatcherTest, SingleCharMatch) {
    // Single character pattern should match correctly
    auto result = FzfMatcher::Match("abcdef", "d");
    EXPECT_TRUE(result.Matched);
    EXPECT_GT(result.Score, 0);
}

TEST(FzfMatcherTest, NoMatchWrongOrder) {
    // Characters in wrong order should not match
    auto result = FzfMatcher::Match("abc", "cba");
    EXPECT_FALSE(result.Matched);
}

TEST(FzfMatcherTest, MatchAtEnd) {
    // Pattern matching at the end of string
    auto result = FzfMatcher::Match("prefix.suffix", "suffix");
    EXPECT_TRUE(result.Matched);
    EXPECT_GT(result.Score, 0);
}
