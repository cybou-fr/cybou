// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_GCS_FILTER_H
#define CYBOU_GCS_FILTER_H

#include <cstdint>
#include <span>
#include <vector>

namespace cybou::gcs {

struct Params {
    uint64_t siphash_k0{0};
    uint64_t siphash_k1{0};
    uint8_t p{0};
    uint32_t m{1};
};

using Element = std::vector<unsigned char>;

/** Encode a set of elements as a deterministic Golomb-coded set. */
std::vector<unsigned char> Build(const Params& params, std::span<const Element> elements);

/** Decode and validate an encoded filter, returning its declared element count. */
uint32_t ElementCount(const Params& params, std::span<const unsigned char> encoded);

/** Check for one element in an encoded filter. */
bool Match(const Params& params, std::span<const unsigned char> encoded, std::span<const unsigned char> element);

/** Check whether any element matches an encoded filter. */
bool MatchAny(const Params& params, std::span<const unsigned char> encoded, std::span<const Element> elements);

} // namespace cybou::gcs

#endif // CYBOU_GCS_FILTER_H
