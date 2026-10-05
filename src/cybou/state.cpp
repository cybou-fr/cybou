// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Детерминированная сериализация и применение переходов канонического состояния.

#include <cybou/state.h>
#include <cybou/protocol_operation.h>
#include <cybou/storage_lease.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string_view>

namespace cybou {
namespace {
constexpr size_t ACCOUNT_SIZE{32 + 8 * 5};
constexpr size_t PUBLICATION_SIZE{32 + 32 + 32 + 4 + 8};
constexpr size_t LEASE_SIZE{32 + 32 + 4 + 1 + 8 + 8 + 8 + 8};
constexpr size_t GENESIS_ALLOCATION_BASE_SIZE{32 + 8 + 4 + 1};

void Write32(std::vector<unsigned char>& out, uint32_t value)
{
    for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

void Write64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

class Reader
{
public:
    explicit Reader(std::span<const unsigned char> bytes) : m_bytes{bytes} {}

    std::optional<uint8_t> U8()
    {
        if (!Remaining()) return std::nullopt;
        return m_bytes[m_offset++];
    }

    std::optional<uint32_t> U32()
    {
        if (Remaining() < 4) return std::nullopt;
        uint32_t result{0};
        for (unsigned i{0}; i < 4; ++i) result |= uint32_t{m_bytes[m_offset++]} << (8 * i);
        return result;
    }

    std::optional<uint64_t> U64()
    {
        if (Remaining() < 8) return std::nullopt;
        uint64_t result{0};
        for (unsigned i{0}; i < 8; ++i) result |= uint64_t{m_bytes[m_offset++]} << (8 * i);
        return result;
    }

    std::optional<std::span<const unsigned char>> Bytes(size_t size)
    {
        if (size > Remaining()) return std::nullopt;
        const auto result = m_bytes.subspan(m_offset, size);
        m_offset += size;
        return result;
    }

    size_t Remaining() const { return m_bytes.size() - m_offset; }

private:
    std::span<const unsigned char> m_bytes;
    size_t m_offset{0};
};

} // namespace

const GenesisAllocation* FindCentralAuthorityAllocation(const CybouState& state)
{
    const GenesisAllocation* result{nullptr};
    for (const auto& [id, allocation] : state.genesis_allocations) {
        if (allocation.label != CENTRAL_AUTHORITY_NAME) continue;
        if (result) return nullptr;
        result = &allocation;
    }
    return result;
}

GenesisAllocation* FindCentralAuthorityAllocation(CybouState& state)
{
    return const_cast<GenesisAllocation*>(FindCentralAuthorityAllocation(std::as_const(state)));
}

namespace {
const uint64_t* CentralAuthorityFeeBalance(const CybouState& state)
{
    // Комиссия всегда имеет единственное canonical destination: либо ещё не
    // заявленная genesis allocation `cybou`, либо уже созданный аккаунт её claimant'а.
    // Любая неоднозначность трактуется как повреждение состояния и останавливает
    // fee routing fail-closed вместо "лучшей попытки".
    const auto* allocation = FindCentralAuthorityAllocation(state);
    if (!allocation) return nullptr;
    if (!allocation->claimed_by) return &allocation->balance;
    const auto account = state.accounts.find(*allocation->claimed_by);
    if (account == state.accounts.end() || !state.identities.Find(*allocation->claimed_by)) return nullptr;
    const auto name = state.names.names.find(allocation->label);
    const auto reverse = state.names.account_names.find(*allocation->claimed_by);
    if (name == state.names.names.end() || name->second != *allocation->claimed_by ||
        reverse == state.names.account_names.end() || reverse->second != allocation->label) return nullptr;
    return &account->second.balance;
}

size_t SerializedStateSize(const CybouState& state,
    const std::vector<unsigned char>& identities,
    const std::vector<unsigned char>& names)
{
    size_t total_size = 4 + state.accounts.size() * ACCOUNT_SIZE + 4 + identities.size() + 4 + names.size() + 4;
    for (const auto& [recovery_id, allocation] : state.genesis_allocations) {
        static_cast<void>(recovery_id);
        total_size += GENESIS_ALLOCATION_BASE_SIZE + allocation.label.size();
        if (allocation.claimed_by) total_size += AccountId::SIZE;
    }
    total_size += 4 + state.publications.size() * PUBLICATION_SIZE;
    total_size += 8 + 8 + 4 + state.leases.size() * LEASE_SIZE;
    return total_size;
}

} // namespace

uint64_t* TreasuryBalance(CybouState& state)
{
    return const_cast<uint64_t*>(CentralAuthorityFeeBalance(std::as_const(state)));
}

uint64_t DebitSystemBalance(AccountState& account, const uint64_t amount)
{
    const uint64_t onboarding = std::min(account.onboarding_system_balance, amount);
    account.onboarding_system_balance -= onboarding;
    account.system_balance -= amount;
    return onboarding;
}

bool CanCreditCentralAuthorityFee(const CybouState& state, uint64_t fee)
{
    const auto* balance = CentralAuthorityFeeBalance(state);
    return balance && *balance <= std::numeric_limits<uint64_t>::max() - fee;
}

bool CreditCentralAuthorityFee(CybouState& state, uint64_t fee)
{
    if (!CanCreditCentralAuthorityFee(state, fee)) return false;
    auto* balance = const_cast<uint64_t*>(CentralAuthorityFeeBalance(std::as_const(state)));
    *balance += fee;
    return true;
}

AccountCreateStateError ApplyAccountCreate(const AccountCreateOp& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state)
{
    // Сначала проверяем уже существующие инварианты registry/state, потому что
    // AccountCreate не должен "починять" испорченный снимок — консенсус может
    // продолжаться только из канонически валидной родительской вершины.
    if (state.accounts.size() != state.identities.Accounts().size()) return AccountCreateStateError::INCONSISTENT_STATE;
    for (const auto& [id, account] : state.accounts) {
        if (!state.identities.Find(id)) return AccountCreateStateError::INCONSISTENT_STATE;
    }
    if (state.accounts.size() >= MAX_IDENTITY_REGISTRY_ACCOUNTS) return AccountCreateStateError::ACCOUNT_LIMIT;
    if (state.accounts.contains(op.account_id)) return AccountCreateStateError::ACCOUNT_EXISTS;
    // Claimant самой Treasury не получает onboarding bonus: он и есть его источник (DEC-277).
    const auto recovery_id = ComputeRecoveryKeyId(op.authorization.recovery_root);
    const auto claim = recovery_id ? state.genesis_allocations.find(*recovery_id) : state.genesis_allocations.end();
    const bool claims_allocation = claim != state.genesis_allocations.end() && !claim->second.claimed_by;
    const bool claims_treasury = claims_allocation && claim->second.label == CENTRAL_AUTHORITY_NAME;
    const uint64_t bonus = claims_treasury ? 0 : params.onboarding_bonus;
    if (bonus) {
        const auto* treasury = TreasuryBalance(state);
        if (!treasury || *treasury < bonus) return AccountCreateStateError::INSUFFICIENT_TREASURY;
    }
    const auto identity_result = state.identities.Register(op, network_binding, block_height, params);
    switch (identity_result) {
    case IdentityRegistryError::NONE: break;
    case IdentityRegistryError::ACCOUNT_EXISTS: return AccountCreateStateError::ACCOUNT_EXISTS;
    case IdentityRegistryError::RECOVERY_KEY_EXISTS: return AccountCreateStateError::RECOVERY_KEY_EXISTS;
    default: return AccountCreateStateError::INVALID_CREATE;
    }
    // Bonus списывается до claim: до него Treasury — сама allocation `cybou`.
    if (bonus) *TreasuryBalance(state) -= bonus;
    uint64_t genesis_balance{0};
    if (claims_allocation) {
        // Identity registry уже отверг повторное использование recovery key,
        // поэтому соответствующая genesis allocation может быть заявлена только один раз.
        genesis_balance = claim->second.balance;
        claim->second.claimed_by = op.account_id;
        if (!claim->second.label.empty()) {
            state.names.names.emplace(claim->second.label, op.account_id);
            state.names.account_names.emplace(op.account_id, claim->second.label);
        }
    }
    state.accounts.emplace(op.account_id, AccountState{
        .balance = genesis_balance,
        .system_balance = bonus,
        .onboarding_system_balance = bonus,
        .creation_height = block_height,
        .creation_epoch = EpochForHeight(block_height, params),
    });
    return AccountCreateStateError::NONE;
}

NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state)
{
    if (op.authorization.kind != IdentityOperationKind::NAME_COMMIT) {
        return NameCommitError::INVALID_AUTHORIZATION;
    }
    const auto expected_payload_commitment = ComputeNameCommitPayloadCommitment(op.commit);
    if (!expected_payload_commitment || op.authorization.payload_commitment != *expected_payload_commitment) {
        return NameCommitError::INVALID_AUTHORIZATION;
    }
    if (!state.accounts.contains(op.authorization.account_id)) {
        return NameCommitError::ACCOUNT_NOT_FOUND;
    }
    if (state.names.account_names.contains(op.authorization.account_id)) {
        return NameCommitError::ACCOUNT_ALREADY_HAS_NAME;
    }
    for (const auto& [existing_commitment, existing_record] : state.names.pending_commits) {
        if (existing_record.account_id == op.authorization.account_id) {
            return NameCommitError::ACCOUNT_HAS_PENDING_COMMIT;
        }
    }
    if (state.names.pending_commits.size() >= params.max_pending_name_commits) {
        return NameCommitError::COMMITMENT_LIMIT_EXCEEDED;
    }
    if (state.names.pending_commits.contains(op.commit.commitment)) {
        return NameCommitError::COMMITMENT_EXISTS;
    }
    if (state.identities.AuthorizeOperation(op.authorization, network_binding) != IdentityRegistryError::NONE) {
        return NameCommitError::INVALID_AUTHORIZATION;
    }

    state.names.pending_commits.emplace(op.commit.commitment, NameCommitRecord{op.authorization.account_id, block_height});
    return NameCommitError::NONE;
}

NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state)
{
    if (op.authorization.kind != IdentityOperationKind::NAME_REVEAL) {
        return NameRevealError::INVALID_AUTHORIZATION;
    }
    const auto expected_payload_commitment = ComputeNameRevealPayloadCommitment(op.reveal);
    if (!expected_payload_commitment || op.authorization.payload_commitment != *expected_payload_commitment) {
        return NameRevealError::INVALID_AUTHORIZATION;
    }
    if (ValidateNameLabel(op.reveal.label) != NameValidationError::NONE) {
        return NameRevealError::INVALID_LABEL_SYNTAX;
    }
    if (!state.accounts.contains(op.authorization.account_id)) {
        return NameRevealError::ACCOUNT_NOT_FOUND;
    }
    if (state.names.account_names.contains(op.authorization.account_id)) {
        return NameRevealError::ACCOUNT_ALREADY_HAS_NAME;
    }
    if (state.names.names.contains(op.reveal.label) ||
        std::ranges::any_of(state.genesis_allocations,
            [&](const auto& entry) { return entry.second.label == op.reveal.label; })) {
        // Имя, уже занятое финализированным аккаунтом или зарезервированное genesis,
        // не может быть переиграно reveal-операцией позже.
        return NameRevealError::NAME_ALREADY_TAKEN;
    }
    const auto expected_commitment = ComputeNameCommitment(network_binding, op.authorization.account_id, op.reveal.label, op.reveal.salt);
    if (op.reveal.work.commitment != expected_commitment) {
        return NameRevealError::INVALID_WORK_PROOF;
    }
    auto commit_it = state.names.pending_commits.find(expected_commitment);
    if (commit_it == state.names.pending_commits.end()) {
        return NameRevealError::COMMITMENT_NOT_FOUND;
    }
    if (commit_it->second.account_id != op.authorization.account_id) {
        return NameRevealError::COMMITMENT_ACCOUNT_MISMATCH;
    }
    if (block_height < commit_it->second.commit_height + params.name_commit_min_depth) {
        return NameRevealError::INSUFFICIENT_COMMIT_DEPTH;
    }
    if (block_height > commit_it->second.commit_height + params.name_commit_max_lifetime) {
        return NameRevealError::COMMIT_EXPIRED;
    }

    // Reveal повторно проверяет и binding commit-а, и актуальность PoW окна:
    // commit depth защищает front-running, а ограниченный epoch не даёт бесконечно
    // переиспользовать старую работу для имён.
    if (op.reveal.work.network_binding != network_binding || op.reveal.work.account_id != op.authorization.account_id) {
        return NameRevealError::INVALID_WORK_PROOF;
    }
    const uint64_t current_epoch = EpochForHeight(block_height, params);
    if (op.reveal.work.work_epoch > current_epoch ||
        op.reveal.work.work_epoch + params.account_creation_epoch_lag < current_epoch) {
        return NameRevealError::INVALID_WORK_PROOF;
    }
    if (!CheckNameClaimWork(op.reveal.work, params.name_claim_work_bits)) {
        return NameRevealError::INVALID_WORK_PROOF;
    }

    if (state.identities.AuthorizeOperation(op.authorization, network_binding) != IdentityRegistryError::NONE) {
        return NameRevealError::INVALID_AUTHORIZATION;
    }

    state.names.pending_commits.erase(commit_it);
    state.names.names.emplace(op.reveal.label, op.authorization.account_id);
    state.names.account_names.emplace(op.authorization.account_id, op.reveal.label);
    return NameRevealError::NONE;
}

RootPublicationError ApplyRootPublication(const AuthorizedRootPublication& op,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params, CybouState& state)
{
    if (op.authorization.kind != IdentityOperationKind::ROOT_PUBLICATION) {
        return RootPublicationError::INVALID_AUTHORIZATION;
    }
    const auto commitment = ComputeRootPublicationPayloadCommitment(op.publication);
    if (!commitment || op.authorization.payload_commitment != *commitment) {
        return RootPublicationError::INVALID_PAYLOAD;
    }
    const auto operation_bytes = SerializeProtocolOperation(ProtocolOperation{op});
    if (!operation_bytes) return RootPublicationError::INVALID_PAYLOAD;
    const auto fee = ComputeRootPublicationFee(params, operation_bytes->size(), op.publication.chunk_count);
    const auto publication_id = ComputeOperationId(*operation_bytes);
    if (!fee || !publication_id || op.publication.lease_periods > params.max_storage_lease_periods) {
        return RootPublicationError::INVALID_PAYLOAD;
    }
    // Начальная аренда оплачивается атомарно с публикацией: контент не бывает финализирован неоплаченным.
    std::optional<uint64_t> escrow{0};
    if (op.publication.lease_periods != 0) {
        escrow = ComputeStorageLeaseEscrow(params, op.publication.chunk_count, params.storage_replica_target,
            op.publication.lease_periods);
    }
    if (!escrow || *escrow > std::numeric_limits<uint64_t>::max() - *fee) return RootPublicationError::INVALID_PAYLOAD;
    auto sender = state.accounts.find(op.authorization.account_id);
    if (sender == state.accounts.end() || !state.identities.Find(op.authorization.account_id)) {
        return RootPublicationError::SENDER_NOT_FOUND;
    }
    if (sender->second.system_balance < *fee + *escrow) return RootPublicationError::INSUFFICIENT_SYSTEM_BALANCE;
    if (!CanCreditCentralAuthorityFee(state, *fee)) return RootPublicationError::FEE_TRANSFER_FAILED;
    if (state.identities.AuthorizeOperation(op.authorization, network_binding) != IdentityRegistryError::NONE) {
        return RootPublicationError::INVALID_AUTHORIZATION;
    }
    DebitSystemBalance(sender->second, *fee);
    CreditCentralAuthorityFee(state, *fee);
    if (*escrow != 0) {
        FundStorageLease(state, *publication_id, op.authorization.account_id, op.publication.chunk_count,
            params.storage_replica_target, op.publication.lease_periods, *escrow);
    }
    return RootPublicationError::NONE;
}

bool RecordPublication(CybouState& state, const cybou::Hash256& publication_id, const AccountId& owner,
    const ChunkId& chunk_authorization_root, uint32_t chunk_count, uint64_t height)
{
    if (publication_id.IsNull() || chunk_count == 0 || state.publications.contains(publication_id) ||
        !state.accounts.contains(owner)) return false;
    state.publications.emplace(publication_id, PublicationRecord{.owner = owner,
        .chunk_authorization_root = chunk_authorization_root, .chunk_count = chunk_count, .height = height});
    return true;
}

RevokePublicationError ApplyRevokePublication(const AuthorizedRevokePublication& op,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params, CybouState& state)
{
    if (op.authorization.kind != IdentityOperationKind::REVOKE_PUBLICATION) {
        return RevokePublicationError::INVALID_AUTHORIZATION;
    }
    const auto commitment = ComputeRevokePublicationPayloadCommitment(op.revoke);
    if (!commitment || op.authorization.payload_commitment != *commitment) return RevokePublicationError::INVALID_PAYLOAD;
    const auto& account_id = op.authorization.account_id;
    auto sender = state.accounts.find(account_id);
    if (sender == state.accounts.end() || !state.identities.Find(account_id)) {
        return RevokePublicationError::SENDER_NOT_FOUND;
    }
    const auto record = state.publications.find(op.revoke.publication_id);
    if (record == state.publications.end()) return RevokePublicationError::PUBLICATION_NOT_FOUND;
    if (record->second.owner != account_id) return RevokePublicationError::NOT_OWNER;
    if (sender->second.system_balance < params.payment_fee) return RevokePublicationError::INSUFFICIENT_SYSTEM_BALANCE;
    if (!CanCreditCentralAuthorityFee(state, params.payment_fee)) return RevokePublicationError::FEE_TRANSFER_FAILED;
    if (state.identities.AuthorizeOperation(op.authorization, network_binding) != IdentityRegistryError::NONE) {
        return RevokePublicationError::INVALID_AUTHORIZATION;
    }
    // Аренда закрывается после текущего несettled периода: финальный settlement
    // ещё оплатит уже оказанную услугу и вернёт остаток escrow плательщику.
    if (auto lease = state.leases.find(op.revoke.publication_id); lease != state.leases.end()) {
        lease->second.end_period = std::min(lease->second.end_period,
            std::max(lease->second.first_period, state.settlement.next_period) + 1);
    }
    state.publications.erase(record);
    DebitSystemBalance(sender->second, params.payment_fee);
    CreditCentralAuthorityFee(state, params.payment_fee);
    return RevokePublicationError::NONE;
}

StateValidationError ValidateCybouState(const CybouState& state, uint64_t* out_total_cybou)
{
    // Проверка состояния намеренно избыточна: state root может считаться только
    // после подтверждения всех взаимных индексов (`accounts`, `identities`, names,
    // genesis labels). Это следует требованиям канонического state из 05_CHAIN_STATE.md.
    if (state.accounts.size() > MAX_IDENTITY_REGISTRY_ACCOUNTS) return StateValidationError::ACCOUNT_LIMIT_EXCEEDED;
    if (state.accounts.size() != state.identities.Accounts().size()) return StateValidationError::ACCOUNT_IDENTITY_COUNT_MISMATCH;
    for (const auto& [id, account] : state.accounts) {
        if (id.IsNull() || !state.identities.Find(id)) return StateValidationError::MISSING_IDENTITY;
        if (account.onboarding_system_balance > account.system_balance) return StateValidationError::BALANCE_OVERFLOW;
    }
    for (const auto& [id, record] : state.identities.Accounts()) {
        const auto root_id = ComputeRecoveryKeyId(record.recovery_key);
        if (!root_id) return StateValidationError::DUPLICATE_RECOVERY_BINDING;
        const auto mapped_acc = state.identities.FindByRecoveryKeyId(*root_id);
        if (!mapped_acc || *mapped_acc != id) return StateValidationError::DUPLICATE_RECOVERY_BINDING;
    }
    if (state.names.names.size() != state.names.account_names.size()) return StateValidationError::INVALID_NAME_REGISTRY;
    if (state.genesis_allocations.size() > MAX_GENESIS_ALLOCATIONS) return StateValidationError::INVALID_NAME_REGISTRY;
    std::set<std::string> allocation_labels;
    std::map<std::string, AccountId> claimed_genesis_labels;
    for (const auto& [recovery_id, allocation] : state.genesis_allocations) {
        static_cast<void>(recovery_id);
        const auto validity = ValidateNameLabel(allocation.label);
        if (!allocation.label.empty() && validity != NameValidationError::NONE &&
            validity != NameValidationError::PROTECTED_NAME) return StateValidationError::INVALID_NAME_REGISTRY;
        if (!allocation.label.empty() && !allocation_labels.insert(allocation.label).second) {
            return StateValidationError::INVALID_NAME_REGISTRY;
        }
        if (allocation.claimed_by && !state.accounts.contains(*allocation.claimed_by)) {
            return StateValidationError::INVALID_NAME_REGISTRY;
        }
        if (allocation.claimed_by && !allocation.label.empty()) {
            claimed_genesis_labels.emplace(allocation.label, *allocation.claimed_by);
        }
    }
    for (const auto& [label, acc] : state.names.names) {
        const auto validity = ValidateNameLabel(label);
        const auto granted = claimed_genesis_labels.find(label);
        if (validity != NameValidationError::NONE &&
            !(validity == NameValidationError::PROTECTED_NAME &&
                granted != claimed_genesis_labels.end() && granted->second == acc)) {
            return StateValidationError::INVALID_NAME_REGISTRY;
        }
        auto it = state.names.account_names.find(acc);
        if (it == state.names.account_names.end() || it->second != label) return StateValidationError::INVALID_NAME_REGISTRY;
        if (!state.accounts.contains(acc)) return StateValidationError::INVALID_NAME_REGISTRY;
    }
    std::set<AccountId> committing_accounts;
    for (const auto& [commit, record] : state.names.pending_commits) {
        if (commit.IsNull() || !state.accounts.contains(record.account_id)) return StateValidationError::INVALID_NAME_REGISTRY;
        if (!committing_accounts.insert(record.account_id).second) return StateValidationError::INVALID_NAME_REGISTRY;
    }
    if (state.names.pending_commits.size() > DEFAULT_MAX_PENDING_NAME_COMMITS) return StateValidationError::INVALID_NAME_REGISTRY;
    const uint64_t total = TotalCybou(state);
    if (out_total_cybou) *out_total_cybou = total;
    if (total == std::numeric_limits<uint64_t>::max()) return StateValidationError::BALANCE_OVERFLOW;
    for (const auto& [id, record] : state.publications) {
        if (id.IsNull() || record.chunk_count == 0 || !state.accounts.contains(record.owner)) {
            return StateValidationError::INVALID_RESOURCE_USAGE;
        }
    }
    // Аренда: плательщик существует, период непуст и начинается не позже курсора settlement.
    for (const auto& [id, lease] : state.leases) {
        if (id.IsNull() || !state.accounts.contains(lease.payer) || lease.units == 0 || lease.replicas == 0 ||
            lease.replicas > MAX_STORAGE_LEASE_REPLICAS || lease.first_period >= lease.end_period ||
            lease.first_period > state.settlement.next_period) return StateValidationError::INVALID_STORAGE_LEASE;
        const auto publication = state.publications.find(id);
        if (publication != state.publications.end() &&
            (publication->second.owner != lease.payer || publication->second.chunk_count != lease.units)) {
            return StateValidationError::INVALID_STORAGE_LEASE;
        }
    }
    return StateValidationError::NONE;
}

uint64_t TotalCybou(const CybouState& state)
{
    constexpr uint64_t OVERFLOW{std::numeric_limits<uint64_t>::max()};
    uint64_t total{0};
    // Сумма строго меньше OVERFLOW: само значение OVERFLOW зарезервировано под переполнение.
    const auto add = [&](uint64_t value) {
        if (value >= OVERFLOW - total) return false;
        total += value;
        return true;
    };
    for (const auto& [id, allocation] : state.genesis_allocations) {
        if (allocation.claimed_by) continue; // counted in the claimant Balance
        if (!add(allocation.balance)) return OVERFLOW;
    }
    for (const auto& [id, account] : state.accounts) {
        if (!add(account.balance) || !add(account.system_balance)) return OVERFLOW;
    }
    for (const auto& [id, lease] : state.leases) {
        if (!add(lease.escrow_onboarding) || !add(lease.escrow_locked)) return OVERFLOW;
    }
    return total;
}

std::optional<std::vector<unsigned char>> SerializeCybouState(const CybouState& state, bool validate)
{
    if (validate && ValidateCybouState(state) != StateValidationError::NONE) return std::nullopt;
    const auto identities = SerializeIdentityRegistry(state.identities);
    if (!identities || identities->size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
    const auto names = SerializeNameRegistry(state.names);
    if (names.size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(SerializedStateSize(state, *identities, names));
    Write32(out, static_cast<uint32_t>(state.accounts.size()));
    for (const auto& [id, account] : state.accounts) {
        if (id.IsNull() || !state.identities.Find(id)) return std::nullopt;
        out.insert(out.end(), id.Value().begin(), id.Value().end());
        Write64(out, account.balance);
        Write64(out, account.system_balance);
        Write64(out, account.onboarding_system_balance);
        Write64(out, account.creation_height);
        Write64(out, account.creation_epoch);
    }
    Write32(out, static_cast<uint32_t>(identities->size()));
    out.insert(out.end(), identities->begin(), identities->end());
    Write32(out, static_cast<uint32_t>(names.size()));
    out.insert(out.end(), names.begin(), names.end());
    Write32(out, static_cast<uint32_t>(state.genesis_allocations.size()));
    for (const auto& [recovery_id, allocation] : state.genesis_allocations) {
        out.insert(out.end(), recovery_id.begin(), recovery_id.end());
        Write64(out, allocation.balance);
        Write32(out, static_cast<uint32_t>(allocation.label.size()));
        out.insert(out.end(), allocation.label.begin(), allocation.label.end());
        out.push_back(allocation.claimed_by ? 1 : 0);
        if (allocation.claimed_by) {
            out.insert(out.end(), allocation.claimed_by->Value().begin(), allocation.claimed_by->Value().end());
        }
    }
    Write32(out, static_cast<uint32_t>(state.publications.size()));
    for (const auto& [id, record] : state.publications) {
        out.insert(out.end(), id.begin(), id.end());
        out.insert(out.end(), record.owner.Value().begin(), record.owner.Value().end());
        out.insert(out.end(), record.chunk_authorization_root.begin(), record.chunk_authorization_root.end());
        Write32(out, record.chunk_count);
        Write64(out, record.height);
    }
    Write64(out, state.settlement.next_period);
    Write64(out, state.settlement.next_period_start_utc);
    Write32(out, static_cast<uint32_t>(state.leases.size()));
    for (const auto& [id, lease] : state.leases) {
        out.insert(out.end(), id.begin(), id.end());
        out.insert(out.end(), lease.payer.Value().begin(), lease.payer.Value().end());
        Write32(out, lease.units);
        out.push_back(lease.replicas);
        Write64(out, lease.first_period);
        Write64(out, lease.end_period);
        Write64(out, lease.escrow_onboarding);
        Write64(out, lease.escrow_locked);
    }
    return out;
}

std::optional<CybouState> DeserializeCybouState(std::span<const unsigned char> bytes)
{
    Reader reader{bytes};
    const auto count = reader.U32();
    if (!count || *count > MAX_IDENTITY_REGISTRY_ACCOUNTS || *count > reader.Remaining() / ACCOUNT_SIZE) return std::nullopt;
    CybouState state{};
    std::optional<AccountId> prior;
    for (uint32_t i{0}; i < *count; ++i) {
        const auto id_bytes = reader.Bytes(AccountId::SIZE);
        if (!id_bytes) return std::nullopt;
        const auto id = AccountId::FromBytes(*id_bytes);
        const auto balance = reader.U64();
        const auto system = reader.U64();
        const auto onboarding = reader.U64();
        const auto height = reader.U64();
        const auto epoch = reader.U64();
        // Map-ключи должны приходить уже в строгом порядке: это закрепляет одну
        // каноническую сериализацию и исключает множественные байтовые представления
        // одного и того же логического состояния.
        if (!id || (prior && !(*prior < *id)) || !balance || !system || !onboarding || !height || !epoch) {
            return std::nullopt;
        }
        prior = *id;
        state.accounts.emplace(*id, AccountState{*balance, *system, *onboarding, *height, *epoch});
    }
    const auto identity_size = reader.U32();
    if (!identity_size) return std::nullopt;
    const auto identity_bytes = reader.Bytes(*identity_size);
    if (!identity_bytes) return std::nullopt;
    auto identities = DeserializeIdentityRegistry(*identity_bytes);
    if (!identities || identities->Accounts().size() != state.accounts.size()) return std::nullopt;
    for (const auto& [id, account] : state.accounts) {
        if (!identities->Find(id)) return std::nullopt;
    }
    state.identities = std::move(*identities);
    const auto names_size = reader.U32();
    if (!names_size) return std::nullopt;
    const auto names_bytes = reader.Bytes(*names_size);
    if (!names_bytes) return std::nullopt;
    const auto names = DeserializeNameRegistry(*names_bytes);
    if (!names) return std::nullopt;
    state.names = *names;
    const auto allocations = reader.U32();
    if (!allocations || *allocations > MAX_GENESIS_ALLOCATIONS) return std::nullopt;
    std::optional<IdentityKeyId> prior_allocation;
    for (uint32_t i{0}; i < *allocations; ++i) {
        const auto id_bytes = reader.Bytes(IdentityKeyId{}.size());
        const auto balance = reader.U64();
        const auto label_size = reader.U32();
        if (!id_bytes || !balance || !label_size || *label_size > NAME_MAX_LABEL_LENGTH) return std::nullopt;
        const auto label = reader.Bytes(*label_size);
        const auto claimed = reader.U8();
        if (!label || !claimed || *claimed > 1) return std::nullopt;
        IdentityKeyId recovery_id{};
        std::copy(id_bytes->begin(), id_bytes->end(), recovery_id.begin());
        if (prior_allocation && !(*prior_allocation < recovery_id)) return std::nullopt;
        prior_allocation = recovery_id;
        GenesisAllocation allocation{.balance = *balance,
            .label = std::string(label->begin(), label->end())};
        if (*claimed) {
            const auto claimant = reader.Bytes(AccountId::SIZE);
            const auto account = claimant ? AccountId::FromBytes(*claimant) : std::nullopt;
            if (!account) return std::nullopt;
            allocation.claimed_by = *account;
        }
        state.genesis_allocations.emplace(recovery_id, std::move(allocation));
    }
    {
        const auto publication_count = reader.U32();
        if (!publication_count || *publication_count > reader.Remaining() / PUBLICATION_SIZE) return std::nullopt;
        std::optional<cybou::Hash256> prior_publication;
        for (uint32_t i{0}; i < *publication_count; ++i) {
            const auto id_bytes = reader.Bytes(32);
            const auto owner_bytes = reader.Bytes(AccountId::SIZE);
            const auto owner = owner_bytes ? AccountId::FromBytes(*owner_bytes) : std::nullopt;
            const auto root_bytes = reader.Bytes(32);
            const auto chunks = reader.U32();
            const auto height = reader.U64();
            if (!id_bytes || !owner || !root_bytes || !chunks || !height) return std::nullopt;
            cybou::Hash256 id;
            std::copy(id_bytes->begin(), id_bytes->end(), id.begin());
            if (prior_publication && !(*prior_publication < id)) return std::nullopt;
            prior_publication = id;
            PublicationRecord record{.owner = *owner, .chunk_count = *chunks, .height = *height};
            std::copy(root_bytes->begin(), root_bytes->end(), record.chunk_authorization_root.begin());
            state.publications.emplace(id, record);
        }
    }
    const auto next_period = reader.U64();
    const auto next_start = reader.U64();
    const auto lease_count = reader.U32();
    if (!next_period || !next_start || !lease_count || *lease_count > reader.Remaining() / LEASE_SIZE) return std::nullopt;
    state.settlement = {.next_period = *next_period, .next_period_start_utc = *next_start};
    std::optional<cybou::Hash256> prior_lease;
    for (uint32_t i{0}; i < *lease_count; ++i) {
        const auto id_bytes = reader.Bytes(32);
        const auto payer_bytes = reader.Bytes(AccountId::SIZE);
        const auto payer = payer_bytes ? AccountId::FromBytes(*payer_bytes) : std::nullopt;
        const auto units = reader.U32();
        const auto replicas = reader.U8();
        const auto first = reader.U64();
        const auto end = reader.U64();
        const auto escrow_onboarding = reader.U64();
        const auto escrow_locked = reader.U64();
        if (!id_bytes || !payer || !units || !replicas || !first || !end || !escrow_onboarding || !escrow_locked) {
            return std::nullopt;
        }
        cybou::Hash256 id;
        std::copy(id_bytes->begin(), id_bytes->end(), id.begin());
        if (prior_lease && !(*prior_lease < id)) return std::nullopt;
        prior_lease = id;
        state.leases.emplace(id, StorageLeaseRecord{.payer = *payer, .units = *units, .replicas = *replicas,
            .first_period = *first, .end_period = *end, .escrow_onboarding = *escrow_onboarding,
            .escrow_locked = *escrow_locked});
    }
    if (reader.Remaining()) return std::nullopt;
    // Окончательная валидация после чтения всех доменов не допускает частично
    // валидных снимков в state store.
    if (ValidateCybouState(state) != StateValidationError::NONE) return std::nullopt;
    return state;
}

std::optional<cybou::Hash256> CybouStateHash(const CybouState& state, bool validate)
{
    constexpr std::string_view domain{"CYBOU/STATE"};
    const auto bytes = SerializeCybouState(state, validate);
    if (!bytes) return std::nullopt;
    cybou::Hash256 hash;
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*bytes}}, hash.begin())) return std::nullopt;
    return hash;
}

} // namespace cybou
