// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <limits>
#include <string_view>

namespace cybou {

std::vector<unsigned char> SerializeCybouState(const CybouState& state)
{
    std::vector<unsigned char> out;
    const auto append_u32le = [&out](const uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    };
    const auto append_u64le = [&out](const uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    };
    const auto append_hash = [&out](const uint256& value) {
        out.insert(out.end(), value.begin(), value.end());
    };

    out.push_back(CYBOU_STATE_VERSION);
    append_u64le(state.onboarding_pool);
    append_u64le(state.security_reward_pool);
    append_u64le(state.pending_fee_pool);
    append_u32le(static_cast<uint32_t>(state.accounts.size()));
    for (const auto& [account_id, acc] : state.accounts) {
        append_hash(account_id.Value());
        append_u64le(acc.balance);
        append_u64le(acc.system_balance);
        append_u64le(acc.creation_height);
        append_u64le(acc.creation_epoch);
        append_hash(acc.active_authorization_key);
        append_u64le(acc.next_nonce);
        append_u64le(acc.last_mail_epoch);
        append_u32le(acc.mail_count_in_epoch);
    }
    return out;
}

std::optional<CybouState> DeserializeCybouState(const std::span<const unsigned char> bytes)
{
    size_t offset{0};
    const auto read_u8 = [&]() -> std::optional<uint8_t> {
        if (offset == bytes.size()) return std::nullopt;
        return bytes[offset++];
    };
    const auto read_u32le = [&]() -> std::optional<uint32_t> {
        if (bytes.size() - offset < 4) return std::nullopt;
        uint32_t value{0};
        for (unsigned i = 0; i < 4; ++i) value |= uint32_t{bytes[offset++]} << (8 * i);
        return value;
    };
    const auto read_u64le = [&]() -> std::optional<uint64_t> {
        if (bytes.size() - offset < 8) return std::nullopt;
        uint64_t value{0};
        for (unsigned i = 0; i < 8; ++i) value |= uint64_t{bytes[offset++]} << (8 * i);
        return value;
    };
    const auto read_hash = [&]() -> std::optional<uint256> {
        if (bytes.size() - offset < uint256::size()) return std::nullopt;
        uint256 value;
        std::copy_n(bytes.begin() + offset, uint256::size(), value.begin());
        offset += uint256::size();
        return value;
    };

    const auto version{read_u8()};
    const auto onboarding_pool{read_u64le()};
    const auto security_pool{read_u64le()};
    const auto pending_pool{read_u64le()};
    const auto account_count{read_u32le()};
    if (!version || *version != CYBOU_STATE_VERSION || !onboarding_pool || !security_pool || !pending_pool ||
        !account_count || *account_count > MAX_SERIALIZED_ACCOUNTS) {
        return std::nullopt;
    }

    CybouState state{
        .onboarding_pool = *onboarding_pool,
        .security_reward_pool = *security_pool,
        .pending_fee_pool = *pending_pool,
        .accounts{},
    };
    for (uint32_t i = 0; i < *account_count; ++i) {
        const auto account_id_bytes{read_hash()};
        const auto balance{read_u64le()};
        const auto system_balance{read_u64le()};
        const auto creation_height{read_u64le()};
        const auto creation_epoch{read_u64le()};
        const auto auth_key{read_hash()};
        const auto next_nonce{read_u64le()};
        const auto last_mail_epoch{read_u64le()};
        const auto mail_count_in_epoch{read_u32le()};
        if (!account_id_bytes || !balance || !system_balance || !creation_height || !creation_epoch ||
            !auth_key || !next_nonce || !last_mail_epoch || !mail_count_in_epoch) {
            return std::nullopt;
        }
        const AccountId account_id{*account_id_bytes};
        if (account_id.IsNull()) return std::nullopt;
        AccountState acc{
            .balance = *balance,
            .system_balance = *system_balance,
            .creation_height = *creation_height,
            .creation_epoch = *creation_epoch,
            .active_authorization_key = *auth_key,
            .next_nonce = *next_nonce,
            .last_mail_epoch = *last_mail_epoch,
            .mail_count_in_epoch = *mail_count_in_epoch,
        };
        if (!state.accounts.emplace(account_id, std::move(acc)).second) return std::nullopt;
    }
    if (offset != bytes.size()) return std::nullopt;
    return state;
}

uint256 CybouStateHash(const CybouState& state)
{
    static constexpr std::string_view DOMAIN{"CYBOU/STATE/V1"};
    const auto bytes{SerializeCybouState(state)};
    uint256 result;
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size());
    hasher.Finalize(result.begin());
    return result;
}

AccountCreateResult ApplyAccountCreate(
    const AccountCreateOpV1& op,
    const uint256& network_id,
    const uint64_t block_height,
    const CybouProtocolParameters& params,
    CybouState& state)
{
    const auto validation_error{ValidateAccountCreateOp(op, network_id, block_height, params)};
    if (validation_error != AccountCreateValidationError::NONE) {
        return {AccountCreateError::INVALID_OP, validation_error};
    }
    if (state.accounts.contains(op.account_id)) {
        return {AccountCreateError::ACCOUNT_ALREADY_EXISTS};
    }
    if (params.onboarding_bonus > 0 && state.onboarding_pool < params.onboarding_bonus) {
        return {AccountCreateError::INSUFFICIENT_ONBOARDING_POOL};
    }

    if (params.onboarding_bonus > 0) {
        state.onboarding_pool -= params.onboarding_bonus;
    }
    AccountState acc{
        .balance = 0,
        .system_balance = params.onboarding_bonus,
        .creation_height = block_height,
        .creation_epoch = EpochForHeight(block_height, params),
        .active_authorization_key = op.initial_authorization.authorization_descriptor,
        .next_nonce = 0,
    };
    state.accounts.emplace(op.account_id, std::move(acc));
    return {};
}

PaymentResult ApplyPayment(
    const AccountId& sender_id,
    const AccountId& recipient_id,
    const uint64_t amount,
    const uint64_t fee,
    CybouState& state)
{
    if (amount == 0) {
        return {PaymentError::ZERO_AMOUNT};
    }
    if (recipient_id.IsNull() || sender_id == recipient_id) {
        return {PaymentError::SELF_PAYMENT};
    }
    auto sender_it{state.accounts.find(sender_id)};
    if (sender_it == state.accounts.end()) {
        return {PaymentError::SENDER_NOT_FOUND};
    }
    auto recipient_it{state.accounts.find(recipient_id)};
    if (recipient_it == state.accounts.end()) {
        return {PaymentError::RECIPIENT_NOT_FOUND};
    }

    if (sender_it->second.balance < amount) {
        return {PaymentError::INSUFFICIENT_BALANCE};
    }
    if (sender_it->second.system_balance < fee) {
        return {PaymentError::INSUFFICIENT_SYSTEM_BALANCE};
    }

    if (recipient_it->second.balance > std::numeric_limits<uint64_t>::max() - amount) {
        return {PaymentError::RECIPIENT_OVERFLOW};
    }

    if (state.pending_fee_pool > std::numeric_limits<uint64_t>::max() - fee) {
        return {PaymentError::FEE_POOL_OVERFLOW};
    }

    sender_it->second.balance -= amount;
    sender_it->second.system_balance -= fee;
    recipient_it->second.balance += amount;
    state.pending_fee_pool += fee;
    sender_it->second.next_nonce++;
    return {};
}

KeyUpdateResult ApplyKeyUpdate(
    const AccountId& account_id,
    const uint256& new_authorization_key,
    CybouState& state)
{
    if (new_authorization_key.IsNull()) {
        return {KeyUpdateError::NULL_KEY};
    }
    auto it{state.accounts.find(account_id)};
    if (it == state.accounts.end()) {
        return {KeyUpdateError::ACCOUNT_NOT_FOUND};
    }
    it->second.active_authorization_key = new_authorization_key;
    it->second.next_nonce++;
    return {};
}

SystemLockResult ApplySystemLock(
    const AccountId& account_id,
    const uint64_t amount,
    CybouState& state)
{
    if (amount == 0) {
        return {SystemLockError::ZERO_AMOUNT};
    }
    auto it{state.accounts.find(account_id)};
    if (it == state.accounts.end()) {
        return {SystemLockError::ACCOUNT_NOT_FOUND};
    }
    if (it->second.balance < amount) {
        return {SystemLockError::INSUFFICIENT_BALANCE};
    }
    if (it->second.system_balance > std::numeric_limits<uint64_t>::max() - amount) {
        return {SystemLockError::SYSTEM_BALANCE_OVERFLOW};
    }
    it->second.balance -= amount;
    it->second.system_balance += amount;
    it->second.next_nonce++;
    return {};
}

uint64_t ComputeProofOfTrustScore(const AccountState& account, uint64_t current_epoch)
{
    // Hard rules in AGENTS.md:
    // - integer arithmetic only
    // - System Balance trust contribution capped
    // - block-height-derived deterministic epoch
    static constexpr uint64_t BASE_SCORE{100};
    static constexpr uint64_t MAX_SYSTEM_BALANCE_CONTRIBUTION{50};
    static constexpr uint64_t MAX_AGE_CONTRIBUTION{100};

    // System balance contribution: 1 point per 100 CYBOU of SystemBalance, capped at 50
    const uint64_t sb_contribution = std::min<uint64_t>(account.system_balance / 100, MAX_SYSTEM_BALANCE_CONTRIBUTION);

    // Account age contribution: 5 points per epoch, capped at 100
    const uint64_t age_epochs = (current_epoch >= account.creation_epoch) ? (current_epoch - account.creation_epoch) : 0;
    const uint64_t age_contribution = std::min<uint64_t>(age_epochs * 5, MAX_AGE_CONTRIBUTION);

    return BASE_SCORE + sb_contribution + age_contribution;
}

uint32_t CalculateMailRateLimit(uint64_t pot_score, const CybouProtocolParameters& params)
{
    if (pot_score < 150) {
        return params.new_account_mail_limit_per_epoch;
    }
    if (pot_score < 200) {
        return DEFAULT_TIER2_MAIL_LIMIT_PER_EPOCH;
    }
    return DEFAULT_TIER3_MAIL_LIMIT_PER_EPOCH;
}

MailResult ApplyMail(
    const AccountId& sender_id,
    const AccountId& recipient_id,
    const uint64_t fee,
    const uint64_t block_height,
    const CybouProtocolParameters& params,
    CybouState& state)
{
    if (recipient_id.IsNull() || sender_id == recipient_id) {
        return {MailError::SELF_MAIL};
    }
    auto sender_it{state.accounts.find(sender_id)};
    if (sender_it == state.accounts.end()) {
        return {MailError::SENDER_NOT_FOUND};
    }
    auto recipient_it{state.accounts.find(recipient_id)};
    if (recipient_it == state.accounts.end()) {
        return {MailError::RECIPIENT_NOT_FOUND};
    }
    if (state.pending_fee_pool > std::numeric_limits<uint64_t>::max() - fee) {
        return {MailError::FEE_POOL_OVERFLOW};
    }

    auto& sender = sender_it->second;
    if (sender.system_balance < fee) {
        return {MailError::INSUFFICIENT_SYSTEM_BALANCE};
    }

    // Rate-limiting check based on PoT epoch
    const uint64_t current_epoch = EpochForHeight(block_height, params);
    if (current_epoch > sender.last_mail_epoch) {
        sender.last_mail_epoch = current_epoch;
        sender.mail_count_in_epoch = 0;
    }

    const uint64_t pot_score = ComputeProofOfTrustScore(sender, current_epoch);
    const uint32_t mail_limit = CalculateMailRateLimit(pot_score, params);

    if (sender.mail_count_in_epoch >= mail_limit) {
        return {MailError::RATE_LIMIT_EXCEEDED};
    }

    sender.mail_count_in_epoch++;
    sender.system_balance -= fee;
    state.pending_fee_pool += fee;
    sender.next_nonce++;
    return {};
}

FeeRoutingResult RoutePendingFees(CybouState& state)
{
    const uint64_t chunks = state.pending_fee_pool / 4;
    const uint64_t remainder = state.pending_fee_pool % 4;

    const uint64_t security_addition = chunks * 3;
    const uint64_t onboarding_addition = chunks * 1;

    if (state.security_reward_pool > std::numeric_limits<uint64_t>::max() - security_addition) {
        return {FeeRoutingError::SECURITY_POOL_OVERFLOW};
    }
    if (state.onboarding_pool > std::numeric_limits<uint64_t>::max() - onboarding_addition) {
        return {FeeRoutingError::ONBOARDING_POOL_OVERFLOW};
    }

    state.security_reward_pool += security_addition;
    state.onboarding_pool += onboarding_addition;
    state.pending_fee_pool = remainder;
    return {};
}

} // namespace cybou
