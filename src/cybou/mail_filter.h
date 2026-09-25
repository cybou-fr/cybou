// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_MAIL_FILTER_H
#define CYBOU_MAIL_FILTER_H

#include <cybou/block.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t MAIL_DISCOVERY_FILTER_VERSION{2};
inline constexpr uint8_t GCS_PARAM_P{19};
inline constexpr uint32_t GCS_PARAM_M{784931};

/**
 * Compute domain-separated recipient discovery tag:
 * SHA256("CYBOU/DISCOVERY_TAG/V2" || recipient_key || salt)
 */
uint256 ComputeRecipientDiscoveryTag(const uint256& recipient_key, const uint256& salt);

/**
 * Compact Mail Discovery Filter.
 *
 * Probabilistic, deterministic Golomb-Coded Set (GCS, BIP 158) containing
 * all recipient discovery tags present in a single block.
 * Keyed by the block ID to prevent cross-block collision manipulation.
 */
struct CybouMailDiscoveryFilter {
    uint8_t version{MAIL_DISCOVERY_FILTER_VERSION};
    uint256 block_id;
    uint32_t num_elements{0};
    std::vector<unsigned char> encoded_filter;

    friend bool operator==(const CybouMailDiscoveryFilter&, const CybouMailDiscoveryFilter&) = default;

    /** Test if a single discovery tag matches the filter. */
    bool Match(const uint256& discovery_tag) const;

    /** Test if any of the provided discovery tags match the filter. */
    bool MatchAny(std::span<const uint256> discovery_tags) const;

    /** Compute domain-separated hash of the filter. */
    uint256 ComputeFilterHash() const;

    /** Compute running filter header. */
    uint256 ComputeFilterHeader(const uint256& prev_filter_header) const;
};

/** Build a compact mail discovery filter from all Mail operations in a block. */
CybouMailDiscoveryFilter BuildBlockMailDiscoveryFilter(const CybouBlock& block);

/** Build a compact mail discovery filter from an explicit set of discovery tags and block ID. */
CybouMailDiscoveryFilter BuildMailDiscoveryFilter(const uint256& block_id, std::span<const uint256> discovery_tags);

std::vector<unsigned char> SerializeMailDiscoveryFilter(const CybouMailDiscoveryFilter& filter);
std::optional<CybouMailDiscoveryFilter> DeserializeMailDiscoveryFilter(std::span<const unsigned char> bytes);

// Transition alias

} // namespace cybou

#endif // CYBOU_MAIL_FILTER_H
