// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_FAST_RANGE_H
#define CYBOU_FAST_RANGE_H

#include <cstdint>

namespace cybou {

/** Map a uint64 value into [0, range) using the high 64 bits of its 128-bit
 * product. A zero range maps to zero, matching the inherited implementation.
 */
inline uint64_t FastRange64(const uint64_t value, const uint64_t range) noexcept
{
    const uint64_t value_hi = value >> 32;
    const uint64_t value_lo = value & 0xFFFFFFFF;
    const uint64_t range_hi = range >> 32;
    const uint64_t range_lo = range & 0xFFFFFFFF;
    const uint64_t high_high = value_hi * range_hi;
    const uint64_t high_low = value_hi * range_lo;
    const uint64_t low_high = value_lo * range_hi;
    const uint64_t low_low = value_lo * range_lo;
    const uint64_t middle = (low_low >> 32) + (low_high & 0xFFFFFFFF) + (high_low & 0xFFFFFFFF);
    return high_high + (low_high >> 32) + (high_low >> 32) + (middle >> 32);
}

} // namespace cybou

#endif // CYBOU_FAST_RANGE_H
