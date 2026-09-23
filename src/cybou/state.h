// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_H
#define CYBOU_STATE_H

#include <cybou/account_creation.h>
#include <cybou/account_id.h>
#include <cybou/protocol_params.h>
#include <uint256.h>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

struct AccountState {
    uint64_t balance{0};
    uint64_t system_balance{0};
    uint64_t creation_height{0};
    uint64_t creation_epoch{0};
    uint256 initial_auth_commitment;
    uint256 active_authorization_key;
    uint64_t next_nonce{0};

    friend bool operator==(const AccountState&, const AccountState&) = default;
};

/** Canonical state tracking monetary pools and accounts. */
struct CybouState {
    uint64_t onboarding_pool{0};
    uint64_t security_reward_pool{0};
    uint64_t pending_fee_pool{0};
    std::map<AccountId, AccountState> accounts;

    friend bool operator==(const CybouState&, const CybouState&) = default;
};

inline constexpr uint8_t CYBOU_STATE_VERSION{1};
inline constexpr uint32_t MAX_SERIALIZED_ACCOUNTS{1'000'000};

std::vector<unsigned char> SerializeCybouState(const CybouState& state);
std::optional<CybouState> DeserializeCybouState(std::span<const unsigned char> bytes);
uint256 CybouStateHash(const CybouState& state);

enum class AccountCreateError : uint8_t {
    NONE,
    INVALID_OP,
    ACCOUNT_ALREADY_EXISTS,
    INSUFFICIENT_ONBOARDING_POOL,
    SYSTEM_BALANCE_OVERFLOW,
};

struct AccountCreateResult {
    AccountCreateError error{AccountCreateError::NONE};
    AccountCreateValidationError validation_error{AccountCreateValidationError::NONE};

    explicit operator bool() const { return error == AccountCreateError::NONE; }
};

/**
 * Apply an AccountCreate operation to a candidate state.
 *
 * The caller owns candidate lifetime: on failure the state is left fully
 * unmodified, so callers executing multi-op blocks operate on a throwaway
 * candidate and commit only on success. The PoT epoch is derived from
 * block_height internally.
 *
 * There is deliberately no undo/rollback counterpart: finalized BFT blocks
 * are never reorged, so production state transition is strictly
 * candidate-validate-commit, not mutate-then-undo.
 */
AccountCreateResult ApplyAccountCreate(
    const AccountCreateOpV1& op,
    const uint256& network_id,
    uint64_t block_height,
    const CybouProtocolParameters& params,
    CybouState& state);

enum class PaymentError : uint8_t {
    NONE,
    SENDER_NOT_FOUND,
    RECIPIENT_NOT_FOUND,
    SELF_PAYMENT,
    ZERO_AMOUNT,
    INSUFFICIENT_BALANCE,
    FEE_CALCULATION_OVERFLOW,
    RECIPIENT_OVERFLOW,
    FEE_POOL_OVERFLOW,
};

struct PaymentResult {
    PaymentError error{PaymentError::NONE};

    explicit operator bool() const { return error == PaymentError::NONE; }
};

PaymentResult ApplyPayment(
    const AccountId& sender_id,
    const AccountId& recipient_id,
    uint64_t amount,
    uint64_t fee,
    CybouState& state);

enum class KeyUpdateError : uint8_t {
    NONE,
    ACCOUNT_NOT_FOUND,
    NULL_KEY,
};

struct KeyUpdateResult {
    KeyUpdateError error{KeyUpdateError::NONE};

    explicit operator bool() const { return error == KeyUpdateError::NONE; }
};

KeyUpdateResult ApplyKeyUpdate(
    const AccountId& account_id,
    const uint256& new_authorization_key,
    CybouState& state);

} // namespace cybou

#endif // CYBOU_STATE_H
