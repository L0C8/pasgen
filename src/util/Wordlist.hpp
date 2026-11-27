#pragma once

#include <cstddef>

namespace pasgen {

// The EFF large wordlist: 7,776 distinct, easy-to-type English words, so each
// word drawn uniformly at random adds log2(7776) = 12.92 bits of entropy.
constexpr std::size_t kEffLargeWordlistSize = 7776;
extern const char* const kEffLargeWordlist[kEffLargeWordlistSize];

} // namespace pasgen
