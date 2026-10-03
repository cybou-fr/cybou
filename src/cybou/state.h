// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Каноническое состояние цепочки CYBOU и детерминированные state-transition helpers.

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

/// \brief Каноническое состояние аккаунта, коммитящееся в корень состояния.
struct AccountState {
    uint64_t balance{0};
    uint64_t system_balance{0};
    uint64_t authority{0};
    uint64_t creation_height{0};
    uint64_t creation_epoch{0};

    friend bool operator==(const AccountState&, const AccountState&) = default;
};

/// \brief Неизменяемое genesis-выделение для recovery-ключа, заявляемое ровно одним AccountCreate.
struct GenesisAllocation {
    uint64_t balance{0};
    uint64_t authority{0};
    std::string label;
    std::optional<AccountId> claimed_by;

    friend bool operator==(const GenesisAllocation&, const GenesisAllocation&) = default;
};

inline constexpr size_t MAX_GENESIS_ALLOCATIONS{16};

/// \brief Полный консенсусный снимок CYBOU, из которого вычисляется state root.
struct CybouState {
    uint64_t onboarding_pool{0};
    std::map<AccountId, AccountState> accounts;
    IdentityRegistry identities;
    NameRegistry names;
    /** Keyed by recovery key id; only claim and pre-claim Central Authority fees mutate it. */
    std::map<IdentityKeyId, GenesisAllocation> genesis_allocations;
};

/// \brief Ищет уникальное genesis-выделение Central Authority; дубликаты считаются некорректным состоянием.
GenesisAllocation* FindCentralAuthorityAllocation(CybouState& state);
/// \copydoc FindCentralAuthorityAllocation(CybouState&)
const GenesisAllocation* FindCentralAuthorityAllocation(const CybouState& state);
/// \brief Проверяет, можно ли безопасно зачислить комиссию Central Authority без переполнения.
bool CanCreditCentralAuthorityFee(const CybouState& state, uint64_t fee);
/// \brief Атомарно зачисляет комиссию Central Authority в текущее место учёта комиссии.
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

/// \brief Применяет AccountCreate к кандидатному состоянию без коммита в хранилище.
AccountCreateStateError ApplyAccountCreate(const AccountCreateOp& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Применяет NameCommit к кандидатному состоянию.
NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Применяет NameReveal к кандидатному состоянию.
NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Результаты валидации и применения RootPublication.
enum class RootPublicationError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    SENDER_NOT_FOUND,
    INSUFFICIENT_SYSTEM_BALANCE,
    FEE_TRANSFER_FAILED,
};

/// \brief Применяет RootPublication и маршрутизацию её комиссии.
RootPublicationError ApplyRootPublication(const AuthorizedRootPublication& op,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params, CybouState& state);

/// \brief Ошибки детерминированной валидации канонического состояния.
enum class StateValidationError : uint8_t {
    NONE,
    ACCOUNT_LIMIT_EXCEEDED,
    ACCOUNT_IDENTITY_COUNT_MISMATCH,
    MISSING_IDENTITY,
    DUPLICATE_RECOVERY_BINDING,
    BALANCE_OVERFLOW,
    INVALID_NAME_REGISTRY,
};

/// \brief Проверяет внутренние инварианты консенсусного состояния.
StateValidationError ValidateCybouState(const CybouState& state);
/// \brief Считает канонический total supply, исключая AUTH и обнаруживая переполнения.
uint64_t TotalSupply(const CybouState& state);

/// \brief Сериализует каноническое состояние в детерминированный бинарный формат.
std::optional<std::vector<unsigned char>> SerializeCybouState(const CybouState& state);
/// \brief Десериализует и валидирует каноническое состояние.
std::optional<CybouState> DeserializeCybouState(std::span<const unsigned char> bytes);
/// \brief Вычисляет domain-separated hash канонического состояния.
std::optional<cybou::Hash256> CybouStateHash(const CybouState& state);

} // namespace cybou
#endif // CYBOU_STATE_H
