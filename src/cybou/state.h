// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_H
#define CYBOU_STATE_H

#include <cybou/economics.h>
#include <cybou/identity_registry.h>
#include <cybou/name_registry.h>
#include <cybou/root_publication.h>
#include <cybou/protocol_params.h>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cybou {

inline constexpr uint8_t CYBOU_STATE_VERSION{12};

struct AccountState {
    uint64_t balance{0};
    uint64_t system_balance{0};
    uint64_t authority{0};
    uint64_t creation_height{0};
    uint64_t creation_epoch{0};

    friend bool operator==(const AccountState&, const AccountState&) = default;
};

/**
 * Genesis-granted Balance, AUTH and name for the Identity whose recovery phrase has
 * this recovery key. Claimed exactly once by that Identity's AccountCreate,
 * which receives `balance` as spendable Balance and `label` as its .cybou
 * name (reserved labels are allowed only here). The record stays after the
 * claim so the reserved-name grant remains verifiable. Only the unique
 * Central Authority allocation may accumulate protocol fees in its Balance
 * before claim; after claim fees credit its AccountState Balance instead.
 */
struct GenesisAllocation {
    uint64_t balance{0};
    uint64_t authority{0};
    std::string label;
    std::optional<AccountId> claimed_by;

    friend bool operator==(const GenesisAllocation&, const GenesisAllocation&) = default;
};

inline constexpr size_t MAX_GENESIS_ALLOCATIONS{16};

struct CybouState {
    uint64_t onboarding_pool{0};
    std::map<AccountId, AccountState> accounts;
    IdentityRegistry identities;
    NameRegistry names;
    /** Keyed by recovery key id; only claim and pre-claim Central Authority fees mutate it. */
    std::map<IdentityKeyId, GenesisAllocation> genesis_allocations;
};

// Duplicate Central Authority labels fail closed.
GenesisAllocation* FindCentralAuthorityAllocation(CybouState& state);
const GenesisAllocation* FindCentralAuthorityAllocation(const CybouState& state);
// Preflight must precede authorization (which advances nonce). Credit is atomic
// and uses the same checks; callers must not mutate the recipient between them.
bool CanCreditCentralAuthorityFee(const CybouState& state, uint64_t fee);
bool CreditCentralAuthorityFee(CybouState& state, uint64_t fee);

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
    const uint256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const uint256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const uint256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

enum class RootPublicationError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    SENDER_NOT_FOUND,
    INSUFFICIENT_SYSTEM_BALANCE,
    FEE_TRANSFER_FAILED,
};

RootPublicationError ApplyRootPublication(const AuthorizedRootPublication& op,
    const uint256& network_binding, const CybouProtocolParameters& params, CybouState& state);

enum class StateValidationError : uint8_t {
    NONE,
    ACCOUNT_LIMIT_EXCEEDED,
    ACCOUNT_IDENTITY_COUNT_MISMATCH,
    MISSING_IDENTITY,
    DUPLICATE_RECOVERY_BINDING,
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
