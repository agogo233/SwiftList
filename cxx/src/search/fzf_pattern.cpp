#include "search/fzf_pattern.h"

#include <cstring>

namespace swiftlist::search {

namespace {

// Returns true if the byte string is all ASCII (no bytes >= 0x80).
bool IsPureAscii(std::string_view s) {
    for (unsigned char c : s) {
        if (c >= 0x80) return false;
    }
    return true;
}

// Lowercase ASCII in place.
void AsciiLowerInPlace(std::string& s) {
    for (auto& c : s) {
        if (static_cast<unsigned char>(c) >= 'A' && static_cast<unsigned char>(c) <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
}

// Compute character bitmask (a-z, 0-9) for a byte string.
uint64_t ComputeCharMask(std::string_view s) {
    uint64_t mask = 0;
    for (unsigned char c : s) {
        if (c >= 'a' && c <= 'z') mask |= (1ULL << (c - 'a'));
        else if (c >= 'A' && c <= 'Z') mask |= (1ULL << (c - 'A'));
        else if (c >= '0' && c <= '9') mask |= (1ULL << (26 + (c - '0')));
    }
    return mask;
}

} // namespace

uint64_t FzfPattern::RequiredMask() const {
    uint64_t mask = 0;
    for (const auto& term : Terms) {
        if (!term.Inverse && !term.Utf8.empty()) {
            mask |= ComputeCharMask(term.Utf8);
        }
    }
    return mask;
}

FzfPattern ParsePattern(std::string_view query, bool caseSensitive) {
    FzfPattern pattern;

    size_t i = 0;
    while (i < query.size()) {
        // Skip whitespace separators.
        while (i < query.size() && (query[i] == ' ' || query[i] == '\t')) ++i;
        if (i >= query.size()) break;

        FzfTerm term;
        term.CaseSensitive = caseSensitive;

        // Check for inverse prefix.
        if (query[i] == '!') {
            term.Inverse = true;
            ++i;
        }

        // Check for exact-match prefix.
        if (i < query.size() && query[i] == '\'') {
            term.Kind = TermKind::Exact;
            ++i;
        }

        // Read the term bytes.
        bool prefixAnchor = false;
        bool suffixAnchor = false;

        if (i < query.size() && query[i] == '^') {
            prefixAnchor = true;
            ++i;
        }

        size_t start = i;
        while (i < query.size() && query[i] != ' ' && query[i] != '\t') {
            ++i;
        }
        term.Utf8 = std::string(query.substr(start, i - start));

        if (!term.Utf8.empty() && term.Utf8.back() == '$') {
            suffixAnchor = true;
            term.Utf8.pop_back();
        }

        // Determine term kind.
        if (prefixAnchor && suffixAnchor) {
            term.Kind = TermKind::Exact;
        } else if (prefixAnchor) {
            term.Kind = TermKind::Prefix;
        } else if (suffixAnchor) {
            term.Kind = TermKind::Suffix;
        }
        // If exact prefix ' was used, kind stays Exact.

        // Lowercase if case-insensitive.
        if (!caseSensitive) {
            AsciiLowerInPlace(term.Utf8);
        }

        // ASCII fast path.
        if (IsPureAscii(term.Utf8)) {
            term.AsciiBytes = term.Utf8;
        }

        pattern.Terms.push_back(std::move(term));
    }

    return pattern;
}

} // namespace swiftlist::search
