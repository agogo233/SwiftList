#include "search/highlight_mask.h"

namespace swiftlist::search {

std::vector<uint64_t> ComputeHighlightMask(int nameLen, int matchStart, int matchEnd) {
    int words = (nameLen + 63) / 64;
    std::vector<uint64_t> mask(words, 0);

    int start = (matchStart < 0) ? 0 : matchStart;
    int end = (matchEnd > nameLen) ? nameLen : matchEnd;

    for (int i = start; i < end; ++i) {
        mask[i >> 6] |= (1ULL << (i & 63));
    }
    return mask;
}

} // namespace swiftlist::search
