// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_V2_H
#define CYBOU_STATE_V2_H

#include <cybou/identity_registry_v2.h>
#include <cybou/name_registry.h>
#include <cybou/validator.h>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t CYBOU_STATE_VERSION_V2{2};

struct AccountStateV2 {
    uint64_t balance{0};
    uint64_t system_balance{0};
    uint64_t creation_height{0};
    uint64_t creation_epoch{0};
    uint64_t last_mail_epoch{0};
    uint32_t mail_count_in_epoch{0};

    friend bool operator==(const AccountStateV2&, const AccountStateV2&) = default;
};

struct CybouStateV2 {
    uint64_t onboarding_pool{0};
    uint64_t security_reward_pool{0};
    uint64_t pending_fee_pool{0};
    std::map<AccountId, AccountStateV2> accounts;
    IdentityRegistryV2 identities;
    ValidatorSetV2 validator_set;
    NameRegistry names;
};

enum class AccountCreateStateErrorV2 : uint8_t {
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
AccountCreateStateErrorV2 ApplyAccountCreateV2(const AccountCreateOpV2& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouStateV2& state);

NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouStateV2& state);

NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouStateV2& state);

enum class StateValidationErrorV2 : uint8_t {
    NONE,
    ACCOUNT_LIMIT_EXCEEDED,
    ACCOUNT_IDENTITY_COUNT_MISMATCH,
    MISSING_IDENTITY,
    DUPLICATE_RECOVERY_BINDING,
    INVALID_VALIDATOR_SET,
    BALANCE_OVERFLOW,
    INVALID_NAME_REGISTRY,
};

StateValidationErrorV2 ValidateCybouStateV2(const CybouStateV2& state);

std::optional<std::vector<unsigned char>> SerializeCybouStateV2(const CybouStateV2& state);
std::optional<CybouStateV2> DeserializeCybouStateV2(std::span<const unsigned char> bytes);
std::optional<uint256> CybouStateHashV2(const CybouStateV2& state);

using StateValidationError = StateValidationErrorV2;

} // namespace cybou
#endif
