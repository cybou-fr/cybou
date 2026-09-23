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
        append_hash(acc.initial_auth_commitment);
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
        const auto auth_commitment{read_hash()};
        if (!account_id_bytes || !balance || !system_balance || !creation_height || !creation_epoch || !auth_commitment) {
            return std::nullopt;
        }
        const AccountId account_id{*account_id_bytes};
        if (account_id.IsNull()) return std::nullopt;
        AccountState acc{
            .balance = *balance,
            .system_balance = *system_balance,
            .creation_height = *creation_height,
            .creation_epoch = *creation_epoch,
            .initial_auth_commitment = *auth_commitment,
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
        .initial_auth_commitment = op.initial_authorization.authorization_descriptor,
    };
    state.accounts.emplace(op.account_id, std::move(acc));
    return {};
}

} // namespace cybou
