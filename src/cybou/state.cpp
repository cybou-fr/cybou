// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state.h>
#include <cybou/protocol_operation.h>
#include <cybou/validation_service.h>
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
    }
    state.accounts.emplace(op.account_id, AccountState{
        .balance = genesis_balance,
        .system_balance = params.onboarding_bonus,
        .creation_height = block_height,
        .creation_epoch = EpochForHeight(block_height, params),
    });
    return AccountCreateStateError::NONE;
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
    std::map<AccountId, size_t> node_counts;
    std::set<std::array<unsigned char,32>> provider_ids;
    for (const auto& [id, node] : state.bound_nodes) {
        const auto* owner = state.identities.Find(node.account);
        if (!owner || owner->key_epoch != node.owner_key_epoch || ValidationNodeId(node.key) != id || ++node_counts[node.account] > 8)
            return StateValidationError::INVALID_NODE_BINDING;
        if (node.provider_key) {
            const auto provider = StorageProviderId(*node.provider_key);
            if (!provider || !provider_ids.insert(*provider).second) return StateValidationError::INVALID_NODE_BINDING;
        }
    }
    if (state.bound_nodes.size() > MAX_IDENTITY_REGISTRY_ACCOUNTS) return StateValidationError::INVALID_NODE_BINDING;
    for (const auto& [id, account] : state.accounts) {
        const auto& a = account.authority;
        if (a.activity_this_epoch > 2048 || a.activity_this_epoch > a.activity || a.protocol_used > 65536 ||
            a.validation_this_epoch > 16 || a.validation_this_epoch > a.validation || a.liveness_observations > 65536 ||
            a.storage_remainder >= AUTHORITY_STORAGE_BYTE_EPOCH_UNIT ||
            a.system_contribution > 100'000'000'000ULL || account.bandwidth_reserved > (256ULL<<30)) return StateValidationError::INVALID_AUTHORITY_STATE;
    }
    if (state.storage_pledges.size() > MAX_IDENTITY_REGISTRY_ACCOUNTS) return StateValidationError::INVALID_AUTHORITY_STATE;
    std::map<AccountId, uint32_t> pledge_counts;
    for (const auto& [key, pledge] : state.storage_pledges) {
        const auto bound = state.bound_nodes.find(pledge.node_id);
        if (key.second == ChunkId{} || bound == state.bound_nodes.end() || bound->second.account != key.first || !bound->second.provider_key || pledge.publication_id.IsNull() ||
            pledge.stored_bytes < ENCRYPTED_CHUNK_MIN_STORED_BYTES || pledge.stored_bytes > ENCRYPTED_CHUNK_MAX_STORED_BYTES || pledge.response_mask > 65535 ||
            ++pledge_counts[key.first] > MAX_STORAGE_PLEDGES_PER_IDENTITY) return StateValidationError::INVALID_AUTHORITY_STATE;
    }
    if (state.resources.size()>MAX_IDENTITY_REGISTRY_ACCOUNTS) return StateValidationError::INVALID_AUTHORITY_STATE;
    std::map<AccountId,size_t> resource_counts;
    std::map<AccountId,uint64_t> storage_bytes, bandwidth_bytes;
    std::set<std::pair<AccountId,uint256>> resource_uses;
    for (const auto& [id,r]:state.resources) {
        if(id.IsNull()||!state.accounts.contains(r.account)||!SerializeResourceReservation({{r.grant}})||
            ++resource_counts[r.account]>MAX_RESOURCE_GRANTS_PER_ACCOUNT || r.epoch>state.accounts.at(r.account).authority.epoch ||
            !resource_uses.emplace(r.account,r.grant.use_commitment).second) return StateValidationError::INVALID_AUTHORITY_STATE;
        auto& total = r.grant.domain==ResourceDomain::STORAGE ? storage_bytes[r.account] : bandwidth_bytes[r.account];
        const auto ceiling = r.grant.domain==ResourceDomain::STORAGE ? (64ULL<<30) : state.accounts.at(r.account).bandwidth_reserved;
        if(total>ceiling || r.grant.bytes>ceiling-total)return StateValidationError::INVALID_AUTHORITY_STATE;
        total+=r.grant.bytes;
    }
    constexpr uint64_t MAX_SUPPLY{100'000'000'000};
    const uint64_t total = TotalSupply(state);
    if (total > MAX_SUPPLY) return StateValidationError::BALANCE_OVERFLOW;
    return StateValidationError::NONE;
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
    out.push_back(CYBOU_STATE_VERSION);
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
        const auto& a = account.authority;
        for (uint64_t value : {a.activity, a.system_contribution, a.penalty_debt, a.epoch, a.activity_this_epoch, a.protocol_used,
                 a.liveness, a.storage, a.storage_remainder}) Write64(out, value);
        Write64(out, a.liveness_epoch); Write64(out, a.liveness_observations); Write64(out, a.last_liveness_height);
        Write64(out, a.validation); Write64(out, a.validation_this_epoch); Write64(out, account.bandwidth_reserved);
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
    Write32(out, static_cast<uint32_t>(state.bound_nodes.size()));
    for (const auto& [id, node] : state.bound_nodes) {
        out.insert(out.end(), id.begin(), id.end());
        out.insert(out.end(), node.account.Value().begin(), node.account.Value().end());
        Write64(out, node.owner_key_epoch);
        out.insert(out.end(), node.key.ed25519.begin(), node.key.ed25519.end());
        out.insert(out.end(), node.key.ml_dsa.begin(), node.key.ml_dsa.end());
        out.push_back(node.provider_key ? 1 : 0);
        if (node.provider_key) {
            out.insert(out.end(),node.provider_key->ed25519.begin(),node.provider_key->ed25519.end());
            out.insert(out.end(),node.provider_key->ml_dsa.begin(),node.provider_key->ml_dsa.end());
        }
    }
    Write32(out, static_cast<uint32_t>(state.storage_pledges.size()));
    for (const auto& [key, pledge] : state.storage_pledges) {
        out.insert(out.end(), key.first.Value().begin(), key.first.Value().end()); out.insert(out.end(), key.second.begin(), key.second.end());
        out.insert(out.end(), pledge.publication_id.begin(), pledge.publication_id.end()); out.insert(out.end(), pledge.node_id.begin(), pledge.node_id.end());
        Write64(out, pledge.stored_bytes); Write64(out, pledge.start_epoch); Write64(out, pledge.epoch);
        Write32(out, pledge.response_mask); out.push_back(pledge.false_claim ? 1 : 0);
    }
    Write32(out, static_cast<uint32_t>(state.resources.size()));
    for (const auto& [id, resource] : state.resources) {
        out.insert(out.end(),id.begin(),id.end()); out.insert(out.end(),resource.account.Value().begin(),resource.account.Value().end());
        out.push_back(static_cast<uint8_t>(resource.grant.domain)); Write64(out,resource.grant.bytes);
        out.insert(out.end(),resource.grant.use_commitment.begin(),resource.grant.use_commitment.end()); Write64(out,resource.epoch);
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
    if (!version || *version != CYBOU_STATE_VERSION || !onboarding || !security || !pending ||
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
        auto& a = state.accounts.at(*id).authority;
        for (auto* field : {&a.activity, &a.system_contribution, &a.penalty_debt, &a.epoch, &a.activity_this_epoch, &a.protocol_used,
                 &a.liveness, &a.storage, &a.storage_remainder}) {
            const auto value = reader.U64(); if (!value) return std::nullopt; *field = *value;
        }
        for (auto* field : {&a.liveness_epoch, &a.liveness_observations, &a.last_liveness_height}) {
            const auto value = reader.U64(); if (!value) return std::nullopt; *field = *value;
        }
        for (auto* field : {&a.validation, &a.validation_this_epoch, &state.accounts.at(*id).bandwidth_reserved}) {
            const auto value = reader.U64(); if (!value) return std::nullopt; *field = *value;
        }
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
    const auto nodes = reader.U32();
    if (!nodes || *nodes > MAX_IDENTITY_REGISTRY_ACCOUNTS || *nodes > reader.Remaining() / (32 + 32 + 8 + 32 + 1312)) return std::nullopt;
    std::optional<uint256> prior_node;
    for (uint32_t i{0}; i < *nodes; ++i) {
        const auto id_bytes = reader.Bytes(32); const auto account_bytes = reader.Bytes(32);
        const auto epoch = reader.U64(); const auto ed = reader.Bytes(32); const auto pq = reader.Bytes(1312);
        if (!id_bytes || !account_bytes || !epoch || !ed || !pq) return std::nullopt;
        uint256 id; std::copy_n(id_bytes->begin(), 32, id.begin());
        const auto account = AccountId::FromBytes(*account_bytes);
        if (!account || id.IsNull() || (prior_node && !(*prior_node < id))) return std::nullopt;
        BoundNode node{*account, {IdentityKeyPurpose::VALIDATION_NODE, {}, {}}, *epoch};
        std::copy_n(ed->begin(), 32, node.key.ed25519.begin()); node.key.ml_dsa.assign(pq->begin(), pq->end());
        const auto provider = reader.U8(); if (!provider || *provider > 1) return std::nullopt;
        if (*provider) {
            const auto provider_ed = reader.Bytes(32); const auto provider_pq = reader.Bytes(1312);
            if (!provider_ed || !provider_pq) return std::nullopt;
            node.provider_key.emplace(IdentityHybridPublicKey{IdentityKeyPurpose::STORAGE_PROVIDER,{}, {}});
            std::copy_n(provider_ed->begin(),32,node.provider_key->ed25519.begin()); node.provider_key->ml_dsa.assign(provider_pq->begin(),provider_pq->end());
        }
        state.bound_nodes.emplace(id, node); prior_node = id;
    }
    const auto pledges = reader.U32();
    if (!pledges || *pledges > MAX_IDENTITY_REGISTRY_ACCOUNTS || *pledges > reader.Remaining() / 157) return std::nullopt;
    std::optional<std::pair<AccountId, ChunkId>> previous_pledge;
    for (uint32_t i{0}; i < *pledges; ++i) {
        const auto account_bytes = reader.Bytes(32); const auto chunk = reader.Bytes(32); const auto publication = reader.Bytes(32); const auto node = reader.Bytes(32);
        const auto size = reader.U64(); const auto start = reader.U64(); const auto epoch = reader.U64(); const auto mask = reader.U32(); const auto false_claim = reader.U8();
        if (!account_bytes || !chunk || !publication || !node || !size || !start || !epoch || !mask || !false_claim || *false_claim > 1) return std::nullopt;
        const auto account = AccountId::FromBytes(*account_bytes); if (!account) return std::nullopt;
        ChunkId id{}; std::copy_n(chunk->begin(), 32, id.begin()); const auto key = std::pair{*account, id};
        if (previous_pledge && !(key > *previous_pledge)) return std::nullopt;
        StoragePledge p; std::copy_n(publication->begin(), 32, p.publication_id.begin()); std::copy_n(node->begin(), 32, p.node_id.begin());
        p.stored_bytes = *size; p.start_epoch = *start; p.epoch = *epoch; p.response_mask = *mask; p.false_claim = *false_claim;
        state.storage_pledges.emplace(key, p); previous_pledge = key;
    }
    const auto resources = reader.U32();
    if (!resources || *resources > MAX_IDENTITY_REGISTRY_ACCOUNTS || *resources > reader.Remaining()/113) return std::nullopt;
    std::optional<uint256> previous_resource;
    for (uint32_t i=0;i<*resources;++i) {
        const auto id_bytes=reader.Bytes(32); const auto owner_bytes=reader.Bytes(32); const auto domain=reader.U8();
        const auto size=reader.U64(); const auto commitment=reader.Bytes(32); const auto epoch=reader.U64();
        if(!id_bytes||!owner_bytes||!domain||!size||!commitment||!epoch) return std::nullopt;
        uint256 id; std::copy_n(id_bytes->begin(),32,id.begin()); const auto owner=AccountId::FromBytes(*owner_bytes);
        if(!owner||id.IsNull()||(previous_resource&&!(*previous_resource<id))) return std::nullopt;
        ReservedResource resource{*owner,{static_cast<ResourceDomain>(*domain),*size,{}},*epoch};
        std::copy_n(commitment->begin(),32,resource.grant.use_commitment.begin()); state.resources.emplace(id,resource); previous_resource=id;
    }
    if (reader.Remaining()) return std::nullopt;
    if (ValidateCybouState(state) != StateValidationError::NONE) return std::nullopt;
    return state;
}

std::optional<uint256> CybouStateHash(const CybouState& state)
{
    constexpr std::string_view domain{"CYBOU/STATE/V6"};
    const auto bytes = SerializeCybouState(state);
    if (!bytes) return std::nullopt;
    uint256 hash;
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*bytes}}, hash.begin())) return std::nullopt;
    return hash;
}

} // namespace cybou
