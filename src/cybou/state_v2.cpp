// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_v2.h>

#include <openssl/evp.h>

#include <algorithm>
#include <limits>
#include <memory>
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
    if (ValidateValidatorSet(state.validator_set) != ValidatorSetValidationError::NONE) return StateValidationErrorV2::INVALID_VALIDATOR_SET;
    constexpr uint64_t MAX_SUPPLY{100'000'000'000};
    uint64_t total{0};
    if (state.onboarding_pool > MAX_SUPPLY) return StateValidationErrorV2::BALANCE_OVERFLOW;
    total += state.onboarding_pool;
    if (state.security_reward_pool > MAX_SUPPLY - total) return StateValidationErrorV2::BALANCE_OVERFLOW;
    total += state.security_reward_pool;
    if (state.pending_fee_pool > MAX_SUPPLY - total) return StateValidationErrorV2::BALANCE_OVERFLOW;
    total += state.pending_fee_pool;
    for (const auto& [id, account] : state.accounts) {
        if (account.balance > MAX_SUPPLY - total) return StateValidationErrorV2::BALANCE_OVERFLOW;
        total += account.balance;
        if (account.system_balance > MAX_SUPPLY - total) return StateValidationErrorV2::BALANCE_OVERFLOW;
        total += account.system_balance;
    }
    return StateValidationErrorV2::NONE;
}

std::optional<std::vector<unsigned char>> SerializeCybouStateV2(const CybouStateV2& state)
{
    if (ValidateCybouStateV2(state) != StateValidationErrorV2::NONE) return std::nullopt;
    const auto identities = SerializeIdentityRegistryV2(state.identities);
    if (!identities || identities->size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
    const auto validators = SerializeValidatorSet(state.validator_set);
    if (validators.size() > std::numeric_limits<uint32_t>::max()) return std::nullopt;
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
    if (!validator_bytes || reader.Remaining()) return std::nullopt;
    const auto validators = DeserializeValidatorSet(*validator_bytes);
    if (!validators || ValidateValidatorSet(*validators) != ValidatorSetValidationError::NONE) return std::nullopt;
    state.validator_set = *validators;
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
