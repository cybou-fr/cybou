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
    uint256 active_authorization_key;
    uint64_t next_nonce{0};
    uint64_t last_mail_epoch{0};
    uint32_t mail_count_in_epoch{0};

    friend bool operator==(const AccountState&, const AccountState&) = default;
};

/**
 * Compute deterministic integer Proof of Trust (PoT) score for an account at current epoch.
 * - Base score: 100
 * - Capped System Balance contribution: min(system_balance / 100, 50)
 * - Account age contribution: min(age_epochs * 5, 100)
 * Range: [100, 250]. Pure integer arithmetic, no floating-point.
 */
uint64_t ComputeProofOfTrustScore(const AccountState& account, uint64_t current_epoch);

/**
 * Calculate the maximum outgoing MailTx allowed in a PoT epoch based on PoT score.
 */
uint32_t CalculateMailRateLimit(uint64_t pot_score, const CybouProtocolParameters& params);

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
    INSUFFICIENT_SYSTEM_BALANCE,
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

enum class SystemLockError : uint8_t {
    NONE,
    ACCOUNT_NOT_FOUND,
    ZERO_AMOUNT,
    INSUFFICIENT_BALANCE,
    SYSTEM_BALANCE_OVERFLOW,
};

struct SystemLockResult {
    SystemLockError error{SystemLockError::NONE};

    explicit operator bool() const { return error == SystemLockError::NONE; }
};

SystemLockResult ApplySystemLock(
    const AccountId& account_id,
    uint64_t amount,
    CybouState& state);

enum class MailError : uint8_t {
    NONE,
    SENDER_NOT_FOUND,
    RECIPIENT_NOT_FOUND,
    SELF_MAIL,
    INSUFFICIENT_SYSTEM_BALANCE,
    FEE_POOL_OVERFLOW,
    RATE_LIMIT_EXCEEDED,
};

struct MailResult {
    MailError error{MailError::NONE};

    explicit operator bool() const { return error == MailError::NONE; }
};

MailResult ApplyMail(
    const AccountId& sender_id,
    const AccountId& recipient_id,
    uint64_t fee,
    uint64_t block_height,
    const CybouProtocolParameters& params,
    CybouState& state);

enum class FeeRoutingError : uint8_t {
    NONE,
    SECURITY_POOL_OVERFLOW,
    ONBOARDING_POOL_OVERFLOW,
};

struct FeeRoutingResult {
    FeeRoutingError error{FeeRoutingError::NONE};

    explicit operator bool() const { return error == FeeRoutingError::NONE; }
};

/**
 * Route indivisible pending fees in 4-CYBOU batches:
 * 75% (3 CYBOU) to SecurityRewardPool, 25% (1 CYBOU) to OnboardingPool.
 * Any remainder (0 <= r < 4) stays in PendingFeePool without rounding loss.
 * Atomic: on overflow returns an error and state is left untouched.
 */
FeeRoutingResult RoutePendingFees(CybouState& state);

} // namespace cybou

#endif // CYBOU_STATE_H
