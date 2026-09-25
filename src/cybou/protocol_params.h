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
inline constexpr uint64_t DEFAULT_MAIL_BASE_FEE{4};
inline constexpr uint64_t DEFAULT_MAIL_TIER_BYTES{1024};
inline constexpr uint64_t DEFAULT_MAIL_TIER_FEE{1};
inline constexpr uint32_t DEFAULT_MAX_MAIL_CIPHERTEXT_SIZE{64 * 1024};
inline constexpr uint32_t DEFAULT_NEW_ACCOUNT_MAIL_LIMIT_PER_EPOCH{25};
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
    uint64_t mail_base_fee{DEFAULT_MAIL_BASE_FEE};
    uint64_t mail_tier_bytes{DEFAULT_MAIL_TIER_BYTES};
    uint64_t mail_tier_fee{DEFAULT_MAIL_TIER_FEE};
    uint32_t max_mail_ciphertext_size{DEFAULT_MAX_MAIL_CIPHERTEXT_SIZE};
    uint32_t new_account_mail_limit_per_epoch{DEFAULT_NEW_ACCOUNT_MAIL_LIMIT_PER_EPOCH};
    uint32_t name_claim_work_bits{DEFAULT_NAME_CLAIM_WORK_BITS};
    uint64_t name_commit_min_depth{DEFAULT_NAME_COMMIT_MIN_DEPTH};
    uint64_t name_commit_max_lifetime{DEFAULT_NAME_COMMIT_MAX_LIFETIME};
    uint32_t max_pending_name_commits{DEFAULT_MAX_PENDING_NAME_COMMITS};

    constexpr uint64_t MailFeeForSize(size_t ciphertext_size) const
    {
        const uint64_t tier_bytes{mail_tier_bytes == 0 ? 1 : mail_tier_bytes};
        const uint64_t tiers = (static_cast<uint64_t>(ciphertext_size) + tier_bytes - 1) / tier_bytes;
        return mail_base_fee + tiers * mail_tier_fee;
    }

    friend bool operator==(const CybouProtocolParameters&, const CybouProtocolParameters&) = default;
};

constexpr uint64_t MailFeeForSize(const size_t ciphertext_size, const CybouProtocolParameters& params)
{
    const uint64_t tier_bytes{params.mail_tier_bytes == 0 ? 1 : params.mail_tier_bytes};
    const uint64_t tiers = (static_cast<uint64_t>(ciphertext_size) + tier_bytes - 1) / tier_bytes;
    return params.mail_base_fee + tiers * params.mail_tier_fee;
}

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
