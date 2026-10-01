// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state.h>
#include <cybou/protocol_operation.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <limits>
#include <set>
#include <string_view>

namespace cybou {
namespace {
constexpr size_t ACCOUNT_SIZE{32 + 8 * 5 + 4};

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

bool BootstrapGrantMapValid(const CybouState& state)
{
    if (!state.legacy_genesis_bootstrap_grants.empty() ||
        state.genesis_bootstrap_grants.size() < MIN_GENESIS_BOOTSTRAP_GRANTS ||
        state.genesis_bootstrap_grants.size() > MAX_GENESIS_BOOTSTRAP_GRANTS) return false;
    std::set<IdentityKeyId> recovery_ids;
    for (const auto& [account_id, grant] : state.genesis_bootstrap_grants) {
        const bool recovery_id_is_zero = std::ranges::all_of(grant.recovery_key_id,
            [](unsigned char byte) { return byte == 0; });
        if (account_id.IsNull() || recovery_id_is_zero || !recovery_ids.insert(grant.recovery_key_id).second ||
            (grant.claimed ? !state.accounts.contains(account_id) : state.accounts.contains(account_id))) return false;
    }
    return true;
}
} // namespace

AccountCreateStateError ApplyAccountCreate(const AccountCreateOp& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state)
{
    if (state.accounts.size() != state.identities.Accounts().size()) return AccountCreateStateError::INCONSISTENT_STATE;
    for (const auto& [id, account] : state.accounts) {
        if (!state.identities.Find(id)) return AccountCreateStateError::INCONSISTENT_STATE;
    }
    if (state.accounts.size() >= MAX_IDENTITY_REGISTRY_ACCOUNTS) return AccountCreateStateError::ACCOUNT_LIMIT;
    if (state.accounts.contains(op.account_id)) return AccountCreateStateError::ACCOUNT_EXISTS;
    const auto bootstrap_grant = state.genesis_bootstrap_grants.find(op.account_id);
    const auto bootstrap_recovery_id = ComputeRecoveryKeyId(op.authorization.recovery_root);
    if (bootstrap_grant != state.genesis_bootstrap_grants.end() &&
        (!bootstrap_recovery_id || bootstrap_grant->second.recovery_key_id != *bootstrap_recovery_id)) {
        return AccountCreateStateError::INVALID_CREATE;
    }
    if (bootstrap_recovery_id && std::ranges::any_of(state.genesis_bootstrap_grants,
        [&](const auto& grant) {
            return grant.second.recovery_key_id == *bootstrap_recovery_id &&
                grant.first != op.account_id && !grant.second.claimed;
        })) return AccountCreateStateError::INVALID_CREATE;
    if (state.onboarding_pool < params.onboarding_bonus) return AccountCreateStateError::INSUFFICIENT_ONBOARDING_POOL;
    const auto identity_result = state.identities.Register(op, network_id, block_height, params);
    switch (identity_result) {
    case IdentityRegistryError::NONE: break;
    case IdentityRegistryError::ACCOUNT_EXISTS: return AccountCreateStateError::ACCOUNT_EXISTS;
    case IdentityRegistryError::RECOVERY_KEY_EXISTS: return AccountCreateStateError::RECOVERY_KEY_EXISTS;
    default: return AccountCreateStateError::INVALID_CREATE;
    }
    state.onboarding_pool -= params.onboarding_bonus;
    uint64_t genesis_balance{0};
    if (const auto recovery_id = ComputeRecoveryKeyId(op.authorization.recovery_root)) {
        if (auto it = state.genesis_allocations.find(*recovery_id);
            it != state.genesis_allocations.end() && !it->second.claimed_by) {
            // Registration above rejects a reused recovery key, so a claim happens once.
            genesis_balance = it->second.balance;
            it->second.claimed_by = op.account_id;
            if (!it->second.label.empty()) {
                state.names.names.emplace(it->second.label, op.account_id);
                state.names.account_names.emplace(op.account_id, it->second.label);
            }
        }
        if (bootstrap_grant != state.genesis_bootstrap_grants.end()) bootstrap_grant->second.claimed = true;
        if (auto legacy_grant = state.legacy_genesis_bootstrap_grants.find(*recovery_id);
            legacy_grant != state.legacy_genesis_bootstrap_grants.end() && !legacy_grant->second) {
            legacy_grant->second = op.account_id;
        }
    }
    state.accounts.emplace(op.account_id, AccountState{
        .balance = genesis_balance,
        .system_balance = params.onboarding_bonus,
        .creation_height = block_height,
        .creation_epoch = EpochForHeight(block_height, params),
    });
    return AccountCreateStateError::NONE;
}

bool CybouState::HasBootstrapGrant(const AccountId& account_id) const
{
    if (account_id.IsNull()) return false;
    if (const auto grant = genesis_bootstrap_grants.find(account_id);
        grant != genesis_bootstrap_grants.end() && grant->second.claimed) return true;
    return std::ranges::any_of(legacy_genesis_bootstrap_grants,
        [&](const auto& grant) { return grant.second == account_id; });
}

std::optional<IdentityHybridPublicKey> CybouState::BootstrapAuthorizationKey(const AccountId& account_id) const
{
    if (!HasBootstrapGrant(account_id)) return std::nullopt;
    const auto* identity = identities.Find(account_id);
    if (!identity || identity->authorization_key.purpose != IdentityKeyPurpose::AUTHORIZATION) return std::nullopt;
    return identity->authorization_key;
}

NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state)
{
    if (op.commit.version != NAME_REGISTRY_VERSION) {
        return NameCommitError::INVALID_PAYLOAD;
    }
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
    if (state.identities.AuthorizeOperation(op.authorization, network_id) != IdentityRegistryError::NONE) {
        return NameCommitError::INVALID_AUTHORIZATION;
    }

    state.names.pending_commits.emplace(op.commit.commitment, NameCommitRecord{op.authorization.account_id, block_height});
    return NameCommitError::NONE;
}

NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state)
{
    if (op.reveal.version != NAME_REGISTRY_VERSION) {
        return NameRevealError::INVALID_PAYLOAD;
    }
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
        return NameRevealError::NAME_ALREADY_TAKEN;
    }
    const auto expected_commitment = ComputeNameCommitment(network_id, op.authorization.account_id, op.reveal.label, op.reveal.salt);
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

    if (op.reveal.work.network_id != network_id || op.reveal.work.account_id != op.authorization.account_id) {
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

    if (state.identities.AuthorizeOperation(op.authorization, network_id) != IdentityRegistryError::NONE) {
        return NameRevealError::INVALID_AUTHORIZATION;
    }

    state.names.pending_commits.erase(commit_it);
    state.names.names.emplace(op.reveal.label, op.authorization.account_id);
    state.names.account_names.emplace(op.authorization.account_id, op.reveal.label);
    return NameRevealError::NONE;
}

RootPublicationError ApplyRootPublication(const AuthorizedRootPublication& op,
    const uint256& network_id, const CybouProtocolParameters& params, CybouState& state)
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
    if (!fee) return RootPublicationError::INVALID_PAYLOAD;
    auto sender = state.accounts.find(op.authorization.account_id);
    if (sender == state.accounts.end() || !state.identities.Find(op.authorization.account_id)) {
        return RootPublicationError::SENDER_NOT_FOUND;
    }
    if (sender->second.system_balance < *fee) return RootPublicationError::INSUFFICIENT_SYSTEM_BALANCE;
    if (state.pending_fee_pool > std::numeric_limits<std::uint64_t>::max() - *fee) {
        return RootPublicationError::FEE_POOL_OVERFLOW;
    }
    if (state.identities.AuthorizeOperation(op.authorization, network_id) != IdentityRegistryError::NONE) {
        return RootPublicationError::INVALID_AUTHORIZATION;
    }
    sender->second.system_balance -= *fee;
    state.pending_fee_pool += *fee;
    return RootPublicationError::NONE;
}

StateValidationError ValidateCybouState(const CybouState& state)
{
    if (state.accounts.size() > MAX_IDENTITY_REGISTRY_ACCOUNTS) return StateValidationError::ACCOUNT_LIMIT_EXCEEDED;
    if (state.accounts.size() != state.identities.Accounts().size()) return StateValidationError::ACCOUNT_IDENTITY_COUNT_MISMATCH;
    for (const auto& [id, account] : state.accounts) {
        if (id.IsNull() || !state.identities.Find(id)) return StateValidationError::MISSING_IDENTITY;
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
    for (const auto& [recovery_id, allocation] : state.genesis_allocations) {
        const auto validity = ValidateNameLabel(allocation.label);
        if (!allocation.label.empty() && validity != NameValidationError::NONE &&
            validity != NameValidationError::RESERVED_NAME) return StateValidationError::INVALID_NAME_REGISTRY;
        if (!allocation.label.empty() && !allocation_labels.insert(allocation.label).second) {
            return StateValidationError::INVALID_NAME_REGISTRY;
        }
        if (allocation.claimed_by && !state.accounts.contains(*allocation.claimed_by)) {
            return StateValidationError::INVALID_NAME_REGISTRY;
        }
    }
    if (!state.genesis_bootstrap_grants.empty() && !BootstrapGrantMapValid(state)) {
        return StateValidationError::INVALID_BOOTSTRAP_GRANTS;
    }
    if (state.legacy_genesis_bootstrap_grants.size() > MAX_LEGACY_GENESIS_BOOTSTRAP_GRANTS ||
        (!state.genesis_bootstrap_grants.empty() && !state.legacy_genesis_bootstrap_grants.empty())) {
        return StateValidationError::INVALID_BOOTSTRAP_GRANTS;
    }
    for (const auto& [recovery_id, claimed_by] : state.legacy_genesis_bootstrap_grants) {
        if (std::ranges::all_of(recovery_id, [](unsigned char byte) { return byte == 0; }) ||
            (claimed_by && !state.accounts.contains(*claimed_by))) {
            return StateValidationError::INVALID_BOOTSTRAP_GRANTS;
        }
    }
    const auto genesis_granted = [&](const std::string& label, const AccountId& acc) {
        return std::ranges::any_of(state.genesis_allocations, [&](const auto& entry) {
            return entry.second.label == label && entry.second.claimed_by == acc;
        });
    };
    for (const auto& [label, acc] : state.names.names) {
        const auto validity = ValidateNameLabel(label);
        if (validity != NameValidationError::NONE &&
            !(validity == NameValidationError::RESERVED_NAME && genesis_granted(label, acc))) {
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
    constexpr uint64_t MAX_SUPPLY{100'000'000'000};
    const uint64_t total = TotalSupply(state);
    if (total > MAX_SUPPLY) return StateValidationError::BALANCE_OVERFLOW;
    return StateValidationError::NONE;
}

bool HasValidGenesisBootstrapRoster(const CybouState& state)
{
    if (!BootstrapGrantMapValid(state)) return false;
    return std::ranges::all_of(state.genesis_bootstrap_grants,
        [](const auto& entry) { return !entry.second.claimed; });
}

bool SetGenesisBootstrapRoster(CybouState& state,
    const std::span<const std::pair<AccountId, IdentityKeyId>> roster)
{
    if (roster.size() < MIN_GENESIS_BOOTSTRAP_GRANTS ||
        roster.size() > MAX_GENESIS_BOOTSTRAP_GRANTS ||
        !state.accounts.empty() || !state.identities.Accounts().empty() ||
        !state.genesis_bootstrap_grants.empty() || !state.legacy_genesis_bootstrap_grants.empty()) return false;

    CybouState candidate{state};
    for (const auto& [account_id, recovery_id] : roster) {
        if (!candidate.genesis_bootstrap_grants.emplace(account_id,
            GenesisBootstrapGrant{.recovery_key_id = recovery_id}).second) return false;
    }
    if (!HasValidGenesisBootstrapRoster(candidate) || ValidateCybouState(candidate) != StateValidationError::NONE) {
        return false;
    }
    state.genesis_bootstrap_grants = std::move(candidate.genesis_bootstrap_grants);
    return true;
}

uint64_t TotalSupply(const CybouState& state)
{
    constexpr uint64_t MAX_SUPPLY{100'000'000'000};
    uint64_t total{0};
    if (state.onboarding_pool > MAX_SUPPLY) return std::numeric_limits<uint64_t>::max();
    total += state.onboarding_pool;
    if (state.security_reward_pool > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
    total += state.security_reward_pool;
    if (state.pending_fee_pool > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
    total += state.pending_fee_pool;
    for (const auto& [id, allocation] : state.genesis_allocations) {
        if (allocation.claimed_by) continue; // counted in the claimant Balance
        if (allocation.balance > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
        total += allocation.balance;
    }
    for (const auto& [id, account] : state.accounts) {
        if (account.balance > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
        total += account.balance;
        if (account.system_balance > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
        total += account.system_balance;
    }
    return total;
}

std::optional<std::vector<unsigned char>> SerializeCybouState(const CybouState& state)
{
    if (ValidateCybouState(state) != StateValidationError::NONE) return std::nullopt;
    const auto identities = SerializeIdentityRegistry(state.identities);
    if (!identities || identities->size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
    const auto names = SerializeNameRegistry(state.names);
    if (names.size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
    std::vector<unsigned char> out;
    // Preserve legacy v7/v8 bytes exactly. The AccountID-keyed target roster
    // opts new states into v9.
    const uint8_t version = !state.genesis_bootstrap_grants.empty() ? CYBOU_STATE_VERSION :
        (!state.legacy_genesis_bootstrap_grants.empty() ? CYBOU_STATE_RECOVERY_GRANT_VERSION :
            CYBOU_STATE_LEGACY_VERSION);
    out.push_back(version);
    Write64(out, state.onboarding_pool);
    Write64(out, state.security_reward_pool);
    Write64(out, state.pending_fee_pool);
    Write32(out, static_cast<uint32_t>(state.accounts.size()));
    for (const auto& [id, account] : state.accounts) {
        if (id.IsNull() || !state.identities.Find(id)) return std::nullopt;
        out.insert(out.end(), id.Value().begin(), id.Value().end());
        Write64(out, account.balance);
        Write64(out, account.system_balance);
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
    if (version == CYBOU_STATE_RECOVERY_GRANT_VERSION) {
        Write32(out, static_cast<uint32_t>(state.legacy_genesis_bootstrap_grants.size()));
        for (const auto& [recovery_id, claimed_by] : state.legacy_genesis_bootstrap_grants) {
            out.insert(out.end(), recovery_id.begin(), recovery_id.end());
            out.push_back(claimed_by ? 1 : 0);
            if (claimed_by) out.insert(out.end(), claimed_by->Value().begin(), claimed_by->Value().end());
        }
    } else if (version == CYBOU_STATE_VERSION) {
        Write32(out, static_cast<uint32_t>(state.genesis_bootstrap_grants.size()));
        for (const auto& [account_id, grant] : state.genesis_bootstrap_grants) {
            out.insert(out.end(), account_id.Value().begin(), account_id.Value().end());
            out.insert(out.end(), grant.recovery_key_id.begin(), grant.recovery_key_id.end());
            out.push_back(grant.claimed ? 1 : 0);
        }
    }
    return out;
}

std::optional<CybouState> DeserializeCybouState(std::span<const unsigned char> bytes)
{
    Reader reader{bytes};
    const auto version = reader.U8();
    const auto onboarding = reader.U64();
    const auto security = reader.U64();
    const auto pending = reader.U64();
    const auto count = reader.U32();
    if (!version || (*version != CYBOU_STATE_LEGACY_VERSION &&
        *version != CYBOU_STATE_RECOVERY_GRANT_VERSION && *version != CYBOU_STATE_VERSION) ||
        !onboarding || !security || !pending ||
        !count || *count > MAX_IDENTITY_REGISTRY_ACCOUNTS || *count > reader.Remaining() / ACCOUNT_SIZE) return std::nullopt;
    CybouState state{};
    state.onboarding_pool = *onboarding;
    state.security_reward_pool = *security;
    state.pending_fee_pool = *pending;
    std::optional<AccountId> prior;
    for (uint32_t i{0}; i < *count; ++i) {
        const auto id_bytes = reader.Bytes(AccountId::SIZE);
        if (!id_bytes) return std::nullopt;
        const auto id = AccountId::FromBytes(*id_bytes);
        const auto balance = reader.U64();
        const auto system = reader.U64();
        const auto height = reader.U64();
        const auto epoch = reader.U64();
        if (!id || (prior && !(*prior < *id)) || !balance || !system || !height || !epoch) return std::nullopt;
        prior = *id;
        state.accounts.emplace(*id, AccountState{*balance, *system, *height, *epoch});
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
        GenesisAllocation allocation{.balance = *balance, .label = std::string(label->begin(), label->end())};
        if (*claimed) {
            const auto claimant = reader.Bytes(AccountId::SIZE);
            const auto account = claimant ? AccountId::FromBytes(*claimant) : std::nullopt;
            if (!account) return std::nullopt;
            allocation.claimed_by = *account;
        }
        state.genesis_allocations.emplace(recovery_id, std::move(allocation));
    }
    if (*version == CYBOU_STATE_RECOVERY_GRANT_VERSION) {
        const auto grants = reader.U32();
        if (!grants || *grants == 0 || *grants > MAX_LEGACY_GENESIS_BOOTSTRAP_GRANTS) return std::nullopt;
        std::optional<IdentityKeyId> prior_grant;
        for (uint32_t i{0}; i < *grants; ++i) {
            const auto id_bytes = reader.Bytes(IdentityKeyId{}.size());
            const auto claimed = reader.U8();
            if (!id_bytes || !claimed || *claimed > 1) return std::nullopt;
            IdentityKeyId recovery_id{};
            std::copy(id_bytes->begin(), id_bytes->end(), recovery_id.begin());
            if (prior_grant && !(*prior_grant < recovery_id)) return std::nullopt;
            prior_grant = recovery_id;
            std::optional<AccountId> claimed_by;
            if (*claimed) {
                const auto claimant = reader.Bytes(AccountId::SIZE);
                const auto account = claimant ? AccountId::FromBytes(*claimant) : std::nullopt;
                if (!account) return std::nullopt;
                claimed_by = *account;
            }
            state.legacy_genesis_bootstrap_grants.emplace(recovery_id, claimed_by);
        }
    } else if (*version == CYBOU_STATE_VERSION) {
        const auto grants = reader.U32();
        if (!grants || *grants < MIN_GENESIS_BOOTSTRAP_GRANTS ||
            *grants > MAX_GENESIS_BOOTSTRAP_GRANTS) return std::nullopt;
        std::optional<AccountId> prior_account;
        for (uint32_t i{0}; i < *grants; ++i) {
            const auto account_bytes = reader.Bytes(AccountId::SIZE);
            const auto recovery_bytes = reader.Bytes(IdentityKeyId{}.size());
            const auto claimed = reader.U8();
            const auto account = account_bytes ? AccountId::FromBytes(*account_bytes) : std::nullopt;
            if (!account || (prior_account && !(*prior_account < *account)) || !recovery_bytes ||
                !claimed || *claimed > 1) return std::nullopt;
            prior_account = *account;
            GenesisBootstrapGrant grant;
            std::copy(recovery_bytes->begin(), recovery_bytes->end(), grant.recovery_key_id.begin());
            grant.claimed = *claimed != 0;
            state.genesis_bootstrap_grants.emplace(*account, std::move(grant));
        }
    }
    if (reader.Remaining()) return std::nullopt;
    if (ValidateCybouState(state) != StateValidationError::NONE) return std::nullopt;
    return state;
}

std::optional<uint256> CybouStateHash(const CybouState& state)
{
    constexpr std::string_view domain{"CYBOU/STATE/V5"};
    const auto bytes = SerializeCybouState(state);
    if (!bytes) return std::nullopt;
    uint256 hash;
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*bytes}}, hash.begin())) return std::nullopt;
    return hash;
}

} // namespace cybou
