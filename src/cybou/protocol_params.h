// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_PARAMS_H
#define CYBOU_PROTOCOL_PARAMS_H

#include <cstdint>

namespace cybou {

inline constexpr uint64_t DEV_ONBOARDING_BONUS{6000};
inline constexpr uint32_t DEFAULT_MAX_ACCOUNT_CREATES_PER_BLOCK{100};
inline constexpr unsigned int DEFAULT_ACCOUNT_CREATION_WORK_BITS{16};
inline constexpr uint64_t DEFAULT_ACCOUNT_CREATION_EPOCH_LAG{1};
inline constexpr uint64_t DEFAULT_EPOCH_BLOCKS{1024};

/**
 * Immutable protocol parameters. For DEV/Beta these are fixed network
 * parameters: every validator derives the identical set from the genesis /
 * network definition. There is intentionally no runtime governance path that
 * mutates consensus parameters — a parameter change is a versioned software
 * upgrade or a new genesis.
 */
struct CybouProtocolParameters {
    unsigned int account_creation_work_bits{DEFAULT_ACCOUNT_CREATION_WORK_BITS};
    uint64_t account_creation_epoch_lag{DEFAULT_ACCOUNT_CREATION_EPOCH_LAG};
    uint32_t max_account_creates_per_block{DEFAULT_MAX_ACCOUNT_CREATES_PER_BLOCK};
    uint64_t onboarding_bonus{DEV_ONBOARDING_BONUS};
    uint64_t epoch_blocks{DEFAULT_EPOCH_BLOCKS};

    friend bool operator==(const CybouProtocolParameters&, const CybouProtocolParameters&) = default;
};

constexpr CybouProtocolParameters DevProtocolParameters()
{
    return CybouProtocolParameters{};
}

/**
 * Canonical PoT epoch derivation. Consensus code must never accept an epoch
 * from a caller: the epoch is always a pure function of the finalized block
 * height and the immutable network parameters.
 */
constexpr uint64_t EpochForHeight(const uint64_t block_height, const CybouProtocolParameters& params)
{
    return params.epoch_blocks == 0 ? 0 : block_height / params.epoch_blocks;
}

} // namespace cybou

#endif // CYBOU_PROTOCOL_PARAMS_H
