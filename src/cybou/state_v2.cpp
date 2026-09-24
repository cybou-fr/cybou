// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_v2.h>

#include <openssl/evp.h>

#include <algorithm>
#include <limits>
#include <memory>
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

AccountCreateStateErrorV2 ApplyAccountCreateV2(const AccountCreateOpV2& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouStateV2& state)
{
    if (state.accounts.size() != state.identities.Accounts().size()) return AccountCreateStateErrorV2::INCONSISTENT_STATE;
    for (const auto& [id, account] : state.accounts) {
        if (!state.identities.Find(id)) return AccountCreateStateErrorV2::INCONSISTENT_STATE;
    }
    if (state.accounts.size() >= MAX_IDENTITY_REGISTRY_ACCOUNTS_V2) return AccountCreateStateErrorV2::ACCOUNT_LIMIT;
    if (state.accounts.contains(op.account_id)) return AccountCreateStateErrorV2::ACCOUNT_EXISTS;
    if (state.onboarding_pool < params.onboarding_bonus) return AccountCreateStateErrorV2::INSUFFICIENT_ONBOARDING_POOL;
    const auto identity_result = state.identities.Register(op, network_id, block_height, params);
    switch (identity_result) {
    case IdentityRegistryErrorV2::NONE: break;
    case IdentityRegistryErrorV2::ACCOUNT_EXISTS: return AccountCreateStateErrorV2::ACCOUNT_EXISTS;
    case IdentityRegistryErrorV2::RECOVERY_KEY_EXISTS: return AccountCreateStateErrorV2::RECOVERY_KEY_EXISTS;
    default: return AccountCreateStateErrorV2::INVALID_CREATE;
    }
    state.onboarding_pool -= params.onboarding_bonus;
    state.accounts.emplace(op.account_id, AccountStateV2{
        .balance = 0,
        .system_balance = params.onboarding_bonus,
        .creation_height = block_height,
        .creation_epoch = EpochForHeight(block_height, params),
    });
    return AccountCreateStateErrorV2::NONE;
}

NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouStateV2& state)
{
    if (op.commit.version != NAME_REGISTRY_VERSION || op.commit.commitment.IsNull()) {
        return NameCommitError::INVALID_PAYLOAD;
    }
    if (op.authorization.kind != DeviceOperationKindV2::NAME_COMMIT) {
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
    for (const auto& [commit_hash, record] : state.names.pending_commits) {
        if (record.account_id == op.authorization.account_id) {
            return NameCommitError::ACCOUNT_HAS_PENDING_COMMIT;
        }
    }
    if (state.names.pending_commits.size() >= params.max_pending_name_commits) {
        return NameCommitError::COMMITMENT_LIMIT_EXCEEDED;
    }
    if (state.names.pending_commits.contains(op.commit.commitment)) {
        return NameCommitError::COMMITMENT_EXISTS;
    }
    if (state.identities.AuthorizeDeviceOperation(op.authorization, network_id) != IdentityRegistryErrorV2::NONE) {
        return NameCommitError::INVALID_AUTHORIZATION;
    }

    state.names.pending_commits.emplace(op.commit.commitment, NameCommitRecord{op.authorization.account_id, block_height});
    return NameCommitError::NONE;
}

NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouStateV2& state)
{
    if (op.reveal.version != NAME_REGISTRY_VERSION) {
        return NameRevealError::INVALID_PAYLOAD;
    }
    if (op.authorization.kind != DeviceOperationKindV2::NAME_REVEAL) {
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
    if (state.names.names.contains(op.reveal.label)) {
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

    if (state.identities.AuthorizeDeviceOperation(op.authorization, network_id) != IdentityRegistryErrorV2::NONE) {
        return NameRevealError::INVALID_AUTHORIZATION;
    }

    state.names.pending_commits.erase(commit_it);
    state.names.names.emplace(op.reveal.label, op.authorization.account_id);
    state.names.account_names.emplace(op.authorization.account_id, op.reveal.label);
    return NameRevealError::NONE;
}

MailError ApplyMail(const AuthorizedMail& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouStateV2& state)
{
    if (op.mail.version != MAIL_TX_VERSION ||
        op.mail.recipient.IsNull() ||
        op.mail.discovery_tag.IsNull() ||
        op.mail.content_commitment.IsNull() ||
        op.mail.ciphertext.empty() ||
        op.mail.ciphertext.size() > params.max_mail_ciphertext_size) {
        return MailError::INVALID_PAYLOAD;
    }
    if (op.authorization.kind != DeviceOperationKindV2::MAIL) {
        return MailError::INVALID_AUTHORIZATION;
    }
    const auto expected_payload_commitment = ComputeMailPayloadCommitment(op.mail);
    if (!expected_payload_commitment || op.authorization.payload_commitment != *expected_payload_commitment) {
        return MailError::INVALID_AUTHORIZATION;
    }
    auto sender_it = state.accounts.find(op.authorization.account_id);
    if (sender_it == state.accounts.end()) {
        return MailError::SENDER_NOT_FOUND;
    }
    if (!state.accounts.contains(op.mail.recipient)) {
        return MailError::RECIPIENT_NOT_FOUND;
    }
    const uint64_t fee = params.MailFeeForSize(op.mail.ciphertext.size());
    if (sender_it->second.system_balance < fee) {
        return MailError::INSUFFICIENT_SYSTEM_BALANCE;
    }
    if (state.pending_fee_pool > std::numeric_limits<uint64_t>::max() - fee) {
        return MailError::FEE_POOL_OVERFLOW;
    }
    const uint64_t current_epoch = EpochForHeight(block_height, params);
    const uint32_t current_count = (sender_it->second.last_mail_epoch == current_epoch)
        ? sender_it->second.mail_count_in_epoch
        : 0;
    if (current_count >= params.new_account_mail_limit_per_epoch) {
        return MailError::MAIL_QUOTA_EXCEEDED;
    }
    if (state.identities.AuthorizeDeviceOperation(op.authorization, network_id) != IdentityRegistryErrorV2::NONE) {
        return MailError::INVALID_AUTHORIZATION;
    }
    sender_it->second.system_balance -= fee;
    state.pending_fee_pool += fee;
    if (sender_it->second.last_mail_epoch == current_epoch) {
        sender_it->second.mail_count_in_epoch += 1;
    } else {
        sender_it->second.last_mail_epoch = current_epoch;
        sender_it->second.mail_count_in_epoch = 1;
    }
    return MailError::NONE;
}

StateValidationErrorV2 ValidateCybouStateV2(const CybouStateV2& state)
{
    if (state.accounts.size() > MAX_IDENTITY_REGISTRY_ACCOUNTS_V2) return StateValidationErrorV2::ACCOUNT_LIMIT_EXCEEDED;
    if (state.accounts.size() != state.identities.Accounts().size()) return StateValidationErrorV2::ACCOUNT_IDENTITY_COUNT_MISMATCH;
    for (const auto& [id, account] : state.accounts) {
        if (id.IsNull() || !state.identities.Find(id)) return StateValidationErrorV2::MISSING_IDENTITY;
    }
    for (const auto& [id, record] : state.identities.Accounts()) {
        const auto root_id = ComputeRecoveryKeyId(record.recovery_root);
        if (!root_id) return StateValidationErrorV2::DUPLICATE_RECOVERY_BINDING;
        const auto mapped_acc = state.identities.FindByRecoveryKeyId(*root_id);
        if (!mapped_acc || *mapped_acc != id) return StateValidationErrorV2::DUPLICATE_RECOVERY_BINDING;
    }
    if (ValidateValidatorSetV2(state.validator_set) != ValidatorSetValidationError::NONE) return StateValidationErrorV2::INVALID_VALIDATOR_SET;
    if (state.names.names.size() != state.names.account_names.size()) return StateValidationErrorV2::INVALID_NAME_REGISTRY;
    for (const auto& [label, acc] : state.names.names) {
        if (ValidateNameLabel(label) != NameValidationError::NONE) return StateValidationErrorV2::INVALID_NAME_REGISTRY;
        auto it = state.names.account_names.find(acc);
        if (it == state.names.account_names.end() || it->second != label) return StateValidationErrorV2::INVALID_NAME_REGISTRY;
        if (!state.accounts.contains(acc)) return StateValidationErrorV2::INVALID_NAME_REGISTRY;
    }
    std::set<AccountId> committing_accounts;
    for (const auto& [commit, record] : state.names.pending_commits) {
        if (commit.IsNull() || !state.accounts.contains(record.account_id)) return StateValidationErrorV2::INVALID_NAME_REGISTRY;
        if (!committing_accounts.insert(record.account_id).second) return StateValidationErrorV2::INVALID_NAME_REGISTRY;
    }
    if (state.names.pending_commits.size() > DEFAULT_MAX_PENDING_NAME_COMMITS) return StateValidationErrorV2::INVALID_NAME_REGISTRY;
    constexpr uint64_t MAX_SUPPLY{100'000'000'000};
    const uint64_t total = TotalSupply(state);
    if (total > MAX_SUPPLY) return StateValidationErrorV2::BALANCE_OVERFLOW;
    return StateValidationErrorV2::NONE;
}

uint64_t TotalSupply(const CybouStateV2& state)
{
    constexpr uint64_t MAX_SUPPLY{100'000'000'000};
    uint64_t total{0};
    if (state.onboarding_pool > MAX_SUPPLY) return std::numeric_limits<uint64_t>::max();
    total += state.onboarding_pool;
    if (state.security_reward_pool > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
    total += state.security_reward_pool;
    if (state.pending_fee_pool > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
    total += state.pending_fee_pool;
    for (const auto& [id, account] : state.accounts) {
        if (account.balance > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
        total += account.balance;
        if (account.system_balance > MAX_SUPPLY - total) return std::numeric_limits<uint64_t>::max();
        total += account.system_balance;
    }
    return total;
}

std::optional<std::vector<unsigned char>> SerializeCybouStateV2(const CybouStateV2& state)
{
    if (ValidateCybouStateV2(state) != StateValidationErrorV2::NONE) return std::nullopt;
    const auto identities = SerializeIdentityRegistryV2(state.identities);
    if (!identities || identities->size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
    const auto validators = SerializeValidatorSetV2(state.validator_set);
    if (validators.size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
    const auto names = SerializeNameRegistry(state.names);
    if (names.size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
    std::vector<unsigned char> out;
    out.push_back(CYBOU_STATE_VERSION_V2);
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
        Write64(out, account.last_mail_epoch);
        Write32(out, account.mail_count_in_epoch);
    }
    Write32(out, static_cast<uint32_t>(identities->size()));
    out.insert(out.end(), identities->begin(), identities->end());
    Write32(out, static_cast<uint32_t>(validators.size()));
    out.insert(out.end(), validators.begin(), validators.end());
    Write32(out, static_cast<uint32_t>(names.size()));
    out.insert(out.end(), names.begin(), names.end());
    return out;
}

std::optional<CybouStateV2> DeserializeCybouStateV2(std::span<const unsigned char> bytes)
{
    Reader reader{bytes};
    const auto version = reader.U8();
    const auto onboarding = reader.U64();
    const auto security = reader.U64();
    const auto pending = reader.U64();
    const auto count = reader.U32();
    if (!version || *version != CYBOU_STATE_VERSION_V2 || !onboarding || !security || !pending ||
        !count || *count > MAX_IDENTITY_REGISTRY_ACCOUNTS_V2 || *count > reader.Remaining() / ACCOUNT_SIZE) return std::nullopt;
    CybouStateV2 state{};
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
        const auto mail_epoch = reader.U64();
        const auto mail_count = reader.U32();
        if (!id || (prior && !(*prior < *id)) || !balance || !system || !height || !epoch || !mail_epoch || !mail_count) return std::nullopt;
        prior = *id;
        state.accounts.emplace(*id, AccountStateV2{*balance, *system, *height, *epoch, *mail_epoch, *mail_count});
    }
    const auto identity_size = reader.U32();
    if (!identity_size) return std::nullopt;
    const auto identity_bytes = reader.Bytes(*identity_size);
    if (!identity_bytes) return std::nullopt;
    auto identities = DeserializeIdentityRegistryV2(*identity_bytes);
    if (!identities || identities->Accounts().size() != state.accounts.size()) return std::nullopt;
    for (const auto& [id, account] : state.accounts) {
        if (!identities->Find(id)) return std::nullopt;
    }
    state.identities = std::move(*identities);
    const auto validator_size = reader.U32();
    if (!validator_size) return std::nullopt;
    const auto validator_bytes = reader.Bytes(*validator_size);
    if (!validator_bytes) return std::nullopt;
    const auto validators = DeserializeValidatorSetV2(*validator_bytes);
    if (!validators || ValidateValidatorSetV2(*validators) != ValidatorSetValidationError::NONE) return std::nullopt;
    state.validator_set = *validators;
    const auto names_size = reader.U32();
    if (!names_size) return std::nullopt;
    const auto names_bytes = reader.Bytes(*names_size);
    if (!names_bytes || reader.Remaining()) return std::nullopt;
    const auto names = DeserializeNameRegistry(*names_bytes);
    if (!names) return std::nullopt;
    state.names = *names;
    if (ValidateCybouStateV2(state) != StateValidationErrorV2::NONE) return std::nullopt;
    return state;
}

std::optional<uint256> CybouStateHashV2(const CybouStateV2& state)
{
    constexpr std::string_view domain{"CYBOU/STATE/V2"};
    const auto bytes = SerializeCybouStateV2(state);
    if (!bytes) return std::nullopt;
    using DigestCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    DigestCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    uint256 hash;
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), bytes->data(), bytes->size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), hash.begin(), &size) != 1 || size != hash.size()) return std::nullopt;
    return hash;
}
} // namespace cybou
