#pragma once

#include <cstdint>
#include <vector>

namespace swiftlist::search {

// Compute a bitmask of which character positions in a UTF-8 name should be highlighted,
// given the matched byte range [start, end).
std::vector<uint64_t> ComputeHighlightMask(int nameLen, int matchStart, int matchEnd);

} // namespace swiftlist::search
