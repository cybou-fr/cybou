// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <limits>
#include <string_view>

namespace cybou {

std::vector<unsigned char> SerializeInviteRedemptionState(const InviteRedemptionState& state)
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

    out.push_back(INVITE_REDEMPTION_STATE_VERSION);
    append_u64le(state.onboarding_pool);
    append_u32le(static_cast<uint32_t>(state.accounts.size()));
    for (const auto& [account_id, balance] : state.accounts) {
        append_hash(account_id.Value());
        append_u64le(balance.balance);
        append_u64le(balance.system_balance);
    }
    append_u32le(static_cast<uint32_t>(state.consumed_voucher_ids.size()));
    for (const auto& voucher_id : state.consumed_voucher_ids) append_hash(voucher_id);
    return out;
}

std::optional<InviteRedemptionState> DeserializeInviteRedemptionState(const std::span<const unsigned char> bytes)
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
    const auto account_count{read_u32le()};
    if (!version || *version != INVITE_REDEMPTION_STATE_VERSION || !onboarding_pool || !account_count ||
        *account_count > MAX_SERIALIZED_ACCOUNTS) return std::nullopt;

    InviteRedemptionState state{
        .onboarding_pool = *onboarding_pool,
        .accounts{},
        .consumed_voucher_ids{},
    };
    for (uint32_t i = 0; i < *account_count; ++i) {
        const auto account_id_bytes{read_hash()};
        const auto balance{read_u64le()};
        const auto system_balance{read_u64le()};
        if (!account_id_bytes || !balance || !system_balance) return std::nullopt;
        const AccountId account_id{*account_id_bytes};
        if (account_id.IsNull() ||
            !state.accounts.emplace(account_id, AccountBalanceState{*balance, *system_balance}).second) return std::nullopt;
    }

    const auto voucher_count{read_u32le()};
    if (!voucher_count || *voucher_count > MAX_SERIALIZED_CONSUMED_VOUCHERS) return std::nullopt;
    for (uint32_t i = 0; i < *voucher_count; ++i) {
        const auto voucher_id{read_hash()};
        if (!voucher_id || voucher_id->IsNull() || !state.consumed_voucher_ids.insert(*voucher_id).second) return std::nullopt;
    }
    if (offset != bytes.size()) return std::nullopt;
    return state;
}

uint256 InviteRedemptionStateHash(const InviteRedemptionState& state)
{
    static constexpr std::string_view DOMAIN{"CYBOU/STATE/INVITE-REDEMPTION/V1"};
    const auto bytes{SerializeInviteRedemptionState(state)};
    uint256 result;
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size());
    hasher.Finalize(result.begin());
    return result;
}

InviteRedemptionResult RedeemInviteVoucher(
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context,
    const OperatorAuthoritySignatureVerifier& verifier,
    InviteRedemptionState& state)
{
    auto validation_context{context};
    validation_context.voucher_already_consumed = state.consumed_voucher_ids.contains(voucher.payload.voucher_id);
    const auto voucher_error{ValidateInviteVoucher(voucher, validation_context, verifier)};
    if (voucher_error != InviteVoucherError::NONE) {
        return {InviteRedemptionError::INVALID_VOUCHER, voucher_error};
    }

    const auto account_it{state.accounts.find(voucher.payload.beneficiary_account_id)};
    if (account_it == state.accounts.end()) return {InviteRedemptionError::ACCOUNT_NOT_FOUND};
    if (state.onboarding_pool < WELCOME_GRANT) return {InviteRedemptionError::INSUFFICIENT_ONBOARDING_POOL};
    if (account_it->second.system_balance > std::numeric_limits<uint64_t>::max() - WELCOME_GRANT) {
        return {InviteRedemptionError::SYSTEM_BALANCE_OVERFLOW};
    }

    // No mutation occurs before every failure path above has completed.
    state.onboarding_pool -= WELCOME_GRANT;
    account_it->second.system_balance += WELCOME_GRANT;
    state.consumed_voucher_ids.insert(voucher.payload.voucher_id);
    return {};
}

} // namespace cybou
