// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state.h>

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
    state.accounts.emplace(op.account_id, AccountState{
        .balance = 0,
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
    const auto validation = ValidateAuthorizedNameCommit(op, network_id, block_height, params);
    if (validation != NameCommitError::NONE) return validation;

    const auto account_id = op.authorization.account_id;
    auto account_it = state.accounts.find(account_id);
    if (account_it == state.accounts.end()) return NameCommitError::ACCOUNT_NOT_FOUND;

    if (!state.identities.Find(account_id)) return NameCommitError::INCONSISTENT_STATE;
    if (state.names.account_names.contains(account_id)) return NameCommitError::ACCOUNT_HAS_NAME;
    if (state.names.pending_commits.size() >= params.max_pending_name_commits) return NameCommitError::COMMIT_LIMIT_REACHED;
    if (state.names.pending_commits.contains(op.commitment)) return NameCommitError::COMMITMENT_EXISTS;

    for (const auto& [existing_commitment, existing_record] : state.names.pending_commits) {
        if (existing_record.account_id == account_id) {
            return NameCommitError::ACCOUNT_HAS_PENDING_COMMIT;
        }
    }

    if (account_it->second.system_balance < params.name_registration_fee) {
        return NameCommitError::INSUFFICIENT_SYSTEM_BALANCE;
    }
    if (state.pending_fee_pool > std::numeric_limits<uint64_t>::max() - params.name_registration_fee) {
        return NameCommitError::FEE_POOL_OVERFLOW;
    }

    const auto auth_err = state.identities.AuthorizeDeviceOperation(op.authorization, network_id);
    if (auth_err != IdentityRegistryError::NONE) return NameCommitError::INVALID_AUTHORIZATION;

    account_it->second.system_balance -= params.name_registration_fee;
    state.pending_fee_pool += params.name_registration_fee;

    PendingNameCommitRecord record{
        .account_id = account_id,
        .device_id = op.authorization.device_id,
        .commit_height = block_height,
    };
    state.names.pending_commits.emplace(op.commitment, std::move(record));

    return NameCommitError::NONE;
}

NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state)
{
    const auto validation = ValidateAuthorizedNameReveal(op, network_id, block_height, params);
    if (validation != NameRevealError::NONE) return validation;

    const auto account_id = op.authorization.account_id;
    auto account_it = state.accounts.find(account_id);
    if (account_it == state.accounts.end()) return NameRevealError::ACCOUNT_NOT_FOUND;

    if (!state.identities.Find(account_id)) return NameRevealError::INCONSISTENT_STATE;
    if (state.names.account_names.contains(account_id)) return NameRevealError::ACCOUNT_HAS_NAME;
    if (state.names.names.contains(op.reveal.label)) return NameRevealError::LABEL_ALREADY_EXISTS;

    const auto expected_commitment = ComputeNameCommitment(
        network_id, account_id, op.reveal.label, op.reveal.salt);
    if (!expected_commitment) return NameRevealError::COMMITMENT_NOT_FOUND;

    auto commit_it = state.names.pending_commits.find(*expected_commitment);
    if (commit_it == state.names.pending_commits.end()) return NameRevealError::COMMITMENT_NOT_FOUND;

    const auto& pending = commit_it->second;
    if (pending.account_id != account_id) return NameRevealError::COMMITMENT_ACCOUNT_MISMATCH;
    if (pending.device_id != op.authorization.device_id) return NameRevealError::COMMITMENT_DEVICE_MISMATCH;

    if (block_height < pending.commit_height + params.name_commit_min_age) {
        return NameRevealError::COMMITMENT_TOO_RECENT;
    }
    if (block_height > pending.commit_height + params.name_commit_max_lifetime) {
        return NameRevealError::COMMITMENT_EXPIRED;
    }

    const auto auth_err = state.identities.AuthorizeDeviceOperation(op.authorization, network_id);
    if (auth_err != IdentityRegistryError::NONE) return NameRevealError::INVALID_AUTHORIZATION;

    state.names.pending_commits.erase(commit_it);
    state.names.names.emplace(op.reveal.label, account_id);
    state.names.account_names.emplace(account_id, op.reveal.label);

    return NameRevealError::NONE;
}

MailError ApplyMail(const AuthorizedMail& op,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state)
{
    const auto validation = ValidateAuthorizedMail(op, network_id, block_height, params);
    if (validation != MailError::NONE) return validation;

    const auto sender_id = op.authorization.account_id;
    auto sender_it = state.accounts.find(sender_id);
    if (sender_it == state.accounts.end()) return MailError::SENDER_NOT_FOUND;

    auto recipient_it = state.accounts.find(op.recipient);
    if (recipient_it == state.accounts.end()) return MailError::RECIPIENT_NOT_FOUND;

    if (!state.identities.Find(sender_id) || !state.identities.Find(op.recipient)) {
        return MailError::INCONSISTENT_STATE;
    }

    const uint64_t current_epoch = EpochForHeight(block_height, params);
    uint32_t current_epoch_count = 0;
    if (sender_it->second.last_mail_epoch == current_epoch) {
        current_epoch_count = sender_it->second.mail_count_in_epoch;
    }

    const uint32_t limit = params.new_account_mail_limit_per_epoch;
    if (current_epoch_count >= limit) {
        return MailError::RATE_LIMIT_EXCEEDED;
    }

    if (sender_it->second.system_balance < op.fee) {
        return MailError::INSUFFICIENT_SYSTEM_BALANCE;
    }
    if (state.pending_fee_pool > std::numeric_limits<uint64_t>::max() - op.fee) {
        return MailError::FEE_POOL_OVERFLOW;
    }

    const auto auth_err = state.identities.AuthorizeDeviceOperation(op.authorization, network_id);
    if (auth_err != IdentityRegistryError::NONE) return MailError::INVALID_AUTHORIZATION;

    sender_it->second.system_balance -= op.fee;
    state.pending_fee_pool += op.fee;

    if (sender_it->second.last_mail_epoch == current_epoch) {
        sender_it->second.mail_count_in_epoch += 1;
    } else {
        sender_it->second.last_mail_epoch = current_epoch;
        sender_it->second.mail_count_in_epoch = 1;
    }
    return MailError::NONE;
}

StateValidationError ValidateCybouState(const CybouState& state)
{
    if (state.accounts.size() > MAX_IDENTITY_REGISTRY_ACCOUNTS) return StateValidationError::ACCOUNT_LIMIT_EXCEEDED;
    if (state.accounts.size() != state.identities.Accounts().size()) return StateValidationError::ACCOUNT_IDENTITY_COUNT_MISMATCH;
    for (const auto& [id, account] : state.accounts) {
        if (id.IsNull() || !state.identities.Find(id)) return StateValidationError::MISSING_IDENTITY;
    }
    for (const auto& [id, record] : state.identities.Accounts()) {
        const auto root_id = ComputeRecoveryKeyId(record.recovery_root);
        if (!root_id) return StateValidationError::DUPLICATE_RECOVERY_BINDING;
        const auto mapped_acc = state.identities.FindByRecoveryKeyId(*root_id);
        if (!mapped_acc || *mapped_acc != id) return StateValidationError::DUPLICATE_RECOVERY_BINDING;
    }
    if (ValidateValidatorSet(state.validator_set) != ValidatorSetValidationError::NONE) return StateValidationError::INVALID_VALIDATOR_SET;
    if (state.names.names.size() != state.names.account_names.size()) return StateValidationError::INVALID_NAME_REGISTRY;
    for (const auto& [label, acc] : state.names.names) {
        if (ValidateNameLabel(label) != NameValidationError::NONE) return StateValidationError::INVALID_NAME_REGISTRY;
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
    const auto validators = SerializeValidatorSet(state.validator_set);
    if (validators.size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
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
        const auto mail_epoch = reader.U64();
        const auto mail_count = reader.U32();
        if (!id || (prior && !(*prior < *id)) || !balance || !system || !height || !epoch || !mail_epoch || !mail_count) return std::nullopt;
        prior = *id;
        state.accounts.emplace(*id, AccountState{*balance, *system, *height, *epoch, *mail_epoch, *mail_count});
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
    const auto validator_size = reader.U32();
    if (!validator_size) return std::nullopt;
    const auto validator_bytes = reader.Bytes(*validator_size);
    if (!validator_bytes) return std::nullopt;
    const auto validators = DeserializeValidatorSet(*validator_bytes);
    if (!validators || ValidateValidatorSet(*validators) != ValidatorSetValidationError::NONE) return std::nullopt;
    state.validator_set = *validators;
    const auto names_size = reader.U32();
    if (!names_size) return std::nullopt;
    const auto names_bytes = reader.Bytes(*names_size);
    if (!names_bytes || reader.Remaining()) return std::nullopt;
    const auto names = DeserializeNameRegistry(*names_bytes);
    if (!names) return std::nullopt;
    state.names = *names;
    if (ValidateCybouState(state) != StateValidationError::NONE) return std::nullopt;
    return state;
}

std::optional<uint256> CybouStateHash(const CybouState& state)
{
    constexpr std::string_view domain{"CYBOU/STATE/V2"};
    const auto bytes = SerializeCybouState(state);
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
