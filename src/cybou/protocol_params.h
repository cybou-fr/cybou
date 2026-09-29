// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_PARAMS_H
#define CYBOU_PROTOCOL_PARAMS_H

#include <cstddef>
#include <cstdint>

namespace cybou {

inline constexpr uint64_t DEV_ONBOARDING_BONUS{6000};
inline constexpr uint32_t DEFAULT_MAX_ACCOUNT_CREATES_PER_BLOCK{100};
inline constexpr uint32_t DEFAULT_ACCOUNT_CREATION_WORK_BITS{16};
inline constexpr uint64_t DEFAULT_ACCOUNT_CREATION_EPOCH_LAG{1};
inline constexpr uint64_t DEFAULT_EPOCH_BLOCKS{1024};
inline constexpr uint64_t DEFAULT_PAYMENT_FEE{1};
inline constexpr uint64_t DEFAULT_ROOT_PUBLICATION_FEE_PER_STARTED_KIB{4};
inline constexpr uint64_t DEFAULT_ROOT_PUBLICATION_FEE_PER_CHUNK{4};
inline constexpr uint32_t DEFAULT_NAME_CLAIM_WORK_BITS{16};
inline constexpr uint64_t DEFAULT_NAME_COMMIT_MIN_DEPTH{1};
inline constexpr uint64_t DEFAULT_NAME_COMMIT_MAX_LIFETIME{1000};
inline constexpr uint32_t DEFAULT_MAX_PENDING_NAME_COMMITS{10000};

/**
 * Immutable protocol parameters. For DEV/Beta these are fixed network
 * parameters: every validator derives the identical set from the genesis /
 * network definition. There is intentionally no runtime governance path that
 * mutates consensus parameters — a parameter change is a versioned software
 * upgrade or a new genesis.
 */
struct CybouProtocolParameters {
    uint32_t account_creation_work_bits{DEFAULT_ACCOUNT_CREATION_WORK_BITS};
    uint64_t account_creation_epoch_lag{DEFAULT_ACCOUNT_CREATION_EPOCH_LAG};
    uint32_t max_account_creates_per_block{DEFAULT_MAX_ACCOUNT_CREATES_PER_BLOCK};
    uint64_t onboarding_bonus{DEV_ONBOARDING_BONUS};
    uint64_t epoch_blocks{DEFAULT_EPOCH_BLOCKS};
    uint64_t payment_fee{DEFAULT_PAYMENT_FEE};
    uint64_t root_publication_fee_per_started_kib{DEFAULT_ROOT_PUBLICATION_FEE_PER_STARTED_KIB};
    uint64_t root_publication_fee_per_chunk{DEFAULT_ROOT_PUBLICATION_FEE_PER_CHUNK};
    uint32_t name_claim_work_bits{DEFAULT_NAME_CLAIM_WORK_BITS};
    uint64_t name_commit_min_depth{DEFAULT_NAME_COMMIT_MIN_DEPTH};
    uint64_t name_commit_max_lifetime{DEFAULT_NAME_COMMIT_MAX_LIFETIME};
    uint32_t max_pending_name_commits{DEFAULT_MAX_PENDING_NAME_COMMITS};
    bool identity_kem_xwing_enabled{false};

    friend bool operator==(const CybouProtocolParameters&, const CybouProtocolParameters&) = default;
};

constexpr CybouProtocolParameters DevProtocolParameters()
{
    CybouProtocolParameters params{};
    params.identity_kem_xwing_enabled = true;
    return params;
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
