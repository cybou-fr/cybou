// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_POA_AUTH_ADJUSTMENT_H
#define CYBOU_POA_AUTH_ADJUSTMENT_H

#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

enum class PoaAuthAction : uint8_t {
    GRANT = 1,
    BURN = 2,
};

/** action || target || amount || block_height || Ed25519 || ML-DSA-65 */
inline constexpr size_t POA_AUTH_ADJUSTMENT_SIZE{1 + 32 + 8 + 8 + 64 + 3309};

/**
 * The only way to change AUTH outside genesis and the flat finalized reward.
 * Signed by the genesis-authorized PoA finalizer key and valid only in the
 * block at `block_height`, which makes it non-replayable without new state.
 */
struct PoaAuthAdjustment {
    PoaAuthAction action{PoaAuthAction::GRANT};
    AccountId target_account_id;
    uint64_t amount{0};
    uint64_t block_height{0};
    IdentityHybridSignature poa_signature;

    friend bool operator==(const PoaAuthAdjustment&, const PoaAuthAdjustment&) = default;
};

enum class PoaAuthAdjustmentError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    WRONG_HEIGHT,
    INVALID_SIGNATURE,
    TARGET_NOT_FOUND,
    AUTHORITY_OVERFLOW,
};

std::optional<std::array<unsigned char, 32>> ComputePoaAuthAdjustmentDigest(
    const cybou::Hash256& network_binding, const PoaAuthAdjustment& adjustment);
std::optional<std::vector<unsigned char>> SerializePoaAuthAdjustment(const PoaAuthAdjustment& adjustment);
std::optional<PoaAuthAdjustment> DeserializePoaAuthAdjustment(std::span<const unsigned char> bytes);

/** GRANT adds `amount`; BURN removes min(AUTH, amount). */
PoaAuthAdjustmentError ApplyPoaAuthAdjustment(const PoaAuthAdjustment& adjustment,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const IdentityHybridPublicKey& poa_key, CybouState& state);

} // namespace cybou
#endif // CYBOU_POA_AUTH_ADJUSTMENT_H
