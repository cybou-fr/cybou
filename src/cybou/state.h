// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_H
#define CYBOU_STATE_H

#include <cybou/identity_registry.h>
#include <cybou/mail_tx.h>
#include <cybou/name_registry.h>
#include <cybou/validator.h>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t CYBOU_STATE_VERSION{2};

struct AccountState {
    uint64_t balance{0};
    uint64_t system_balance{0};
    uint64_t creation_height{0};
    uint64_t creation_epoch{0};
    uint64_t last_mail_epoch{0};
    uint32_t mail_count_in_epoch{0};

    friend bool operator==(const AccountState&, const AccountState&) = default;
};

struct CybouState {
    uint64_t onboarding_pool{0};
    uint64_t security_reward_pool{0};
    uint64_t pending_fee_pool{0};
    std::map<AccountId, AccountState> accounts;
    IdentityRegistry identities;
    ValidatorSet validator_set;
    NameRegistry names;
};

enum class AccountCreateStateError : uint8_t {
    NONE,
    INVALID_CREATE,
    ACCOUNT_EXISTS,
    RECOVERY_KEY_EXISTS,
    ACCOUNT_LIMIT,
    INSUFFICIENT_ONBOARDING_POOL,
    INCONSISTENT_STATE,
};

// The caller applies this to a candidate state and commits after the entire
// block succeeds. On a validation error the supplied state is unchanged.
AccountCreateStateError ApplyAccountCreate(const AccountCreateOp& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

MailError ApplyMail(const AuthorizedMail& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

enum class StateValidationError : uint8_t {
    NONE,
    ACCOUNT_LIMIT_EXCEEDED,
    ACCOUNT_IDENTITY_COUNT_MISMATCH,
    MISSING_IDENTITY,
    DUPLICATE_RECOVERY_BINDING,
    INVALID_VALIDATOR_SET,
    BALANCE_OVERFLOW,
    INVALID_NAME_REGISTRY,
};

StateValidationError ValidateCybouState(const CybouState& state);
uint64_t TotalSupply(const CybouState& state);

std::optional<std::vector<unsigned char>> SerializeCybouState(const CybouState& state);
std::optional<CybouState> DeserializeCybouState(std::span<const unsigned char> bytes);
std::optional<uint256> CybouStateHash(const CybouState& state);

} // namespace cybou
#endif // CYBOU_STATE_H
