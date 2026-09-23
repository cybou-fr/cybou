// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_H
#define CYBOU_STATE_H

#include <cybou/account_creation.h>
#include <cybou/account_id.h>
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

struct CybouStateDelta {
    std::vector<AccountId> created_accounts;
    uint64_t onboarding_pool_debited{0};
    uint64_t system_balance_credited{0};

    friend bool operator==(const CybouStateDelta&, const CybouStateDelta&) = default;
};

inline constexpr uint8_t CYBOU_STATE_VERSION{3};
inline constexpr uint64_t DEV_ONBOARDING_BONUS{6000};
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

AccountCreateResult ApplyAccountCreate(
    const AccountCreateOpV1& op,
    const uint256& network_id,
    uint64_t block_height,
    uint64_t epoch,
    unsigned int required_work_bits,
    uint64_t onboarding_bonus,
    CybouState& state,
    CybouStateDelta& delta);

void UndoAccountCreateDelta(const CybouStateDelta& delta, CybouState& state);

} // namespace cybou

#endif // CYBOU_STATE_H
