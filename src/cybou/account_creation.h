// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ACCOUNT_CREATION_H
#define CYBOU_ACCOUNT_CREATION_H

#include <cybou/account_id.h>
#include <cybou/identity_authorization.h>
#include <cybou/protocol_params.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace cybou {

inline constexpr size_t ACCOUNT_CREATE_WORK_SIZE{113};
inline constexpr size_t ACCOUNT_CREATE_SIZE{9334};

struct AccountCreationWork {
    uint256 network_id;
    AccountId account_id;
    std::array<unsigned char, 32> authorization_commitment{};
    uint64_t work_epoch{0};
    uint64_t nonce{0};

    friend bool operator==(const AccountCreationWork&, const AccountCreationWork&) = default;
};

struct AccountCreateOp {
    AccountId account_id;
    IdentityAuthorization authorization;
    AccountCreationWork work;
    IdentityHybridSignature recovery_pop;
    IdentityHybridSignature device_pop;

    friend bool operator==(const AccountCreateOp&, const AccountCreateOp&) = default;
};

enum class AccountCreateError : uint8_t {
    NONE,
    INVALID_FORMAT,
    NULL_ACCOUNT_ID,
    NETWORK_MISMATCH,
    ACCOUNT_ID_MISMATCH,
    COMMITMENT_MISMATCH,
    FUTURE_WORK_EPOCH,
    EXPIRED_WORK_EPOCH,
    INSUFFICIENT_WORK,
    INVALID_RECOVERY_POP,
    INVALID_DEVICE_POP,
};

std::optional<std::array<unsigned char, ACCOUNT_CREATE_WORK_SIZE>> SerializeAccountCreationWork(
    const AccountCreationWork& work);
std::optional<std::array<unsigned char, ACCOUNT_CREATE_SIZE>> SerializeAccountCreateOp(
    const AccountCreateOp& op);
std::optional<AccountCreateOp> DeserializeAccountCreateOp(std::span<const unsigned char> bytes);
std::optional<std::array<unsigned char, 32>> ComputeAccountCreateWorkHash(const AccountCreationWork& work);
std::optional<std::array<unsigned char, 32>> ComputeAccountCreatePopDigest(
    const uint256& network_id, const AccountId& account_id,
    const IdentityAuthorization& authorization);
AccountCreateError ValidateAccountCreateOp(
    const AccountCreateOp& op, const uint256& network_id,
    uint64_t block_height, const CybouProtocolParameters& params);

inline bool CheckAccountCreationWork(const AccountCreationWork& work, unsigned required_bits)
{
    const auto hash = ComputeAccountCreateWorkHash(work);
    if (!hash) return false;
    if (required_bits > 256) return false;
    unsigned count{0};
    for (const unsigned char byte : *hash) {
        if (byte == 0) { count += 8; continue; }
        count += std::countl_zero(byte);
        break;
    }
    return count >= required_bits;
}

// Transition aliases
inline constexpr size_t ACCOUNT_CREATE_V2_WORK_SIZE{ACCOUNT_CREATE_WORK_SIZE};
inline constexpr size_t ACCOUNT_CREATE_V2_SIZE{ACCOUNT_CREATE_SIZE};
using AccountCreationWorkV2 = AccountCreationWork;
using AccountCreateOpV2 = AccountCreateOp;
using AccountCreateV2Error = AccountCreateError;
using AccountCreateValidationError = AccountCreateError;

inline std::optional<std::array<unsigned char, ACCOUNT_CREATE_WORK_SIZE>> SerializeAccountCreationWorkV2(
    const AccountCreationWork& work) { return SerializeAccountCreationWork(work); }
inline std::optional<std::array<unsigned char, ACCOUNT_CREATE_SIZE>> SerializeAccountCreateOpV2(
    const AccountCreateOp& op) { return SerializeAccountCreateOp(op); }
inline std::optional<AccountCreateOp> DeserializeAccountCreateOpV2(std::span<const unsigned char> bytes) {
    return DeserializeAccountCreateOp(bytes);
}
inline std::optional<std::array<unsigned char, 32>> ComputeAccountCreateWorkHashV2(const AccountCreationWork& work) {
    return ComputeAccountCreateWorkHash(work);
}
inline std::optional<std::array<unsigned char, 32>> ComputeAccountCreatePopDigestV2(
    const uint256& network_id, const AccountId& account_id, const IdentityAuthorization& authorization) {
    return ComputeAccountCreatePopDigest(network_id, account_id, authorization);
}
inline AccountCreateError ValidateAccountCreateOpV2(
    const AccountCreateOp& op, const uint256& network_id, uint64_t block_height, const CybouProtocolParameters& params) {
    return ValidateAccountCreateOp(op, network_id, block_height, params);
}

} // namespace cybou

#endif // CYBOU_ACCOUNT_CREATION_H
