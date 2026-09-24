// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ACCOUNT_CREATION_V2_H
#define CYBOU_ACCOUNT_CREATION_V2_H

#include <cybou/account_id.h>
#include <cybou/identity_authorization_v2.h>
#include <cybou/protocol_params.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace cybou {

inline constexpr size_t ACCOUNT_CREATE_V2_WORK_SIZE{113};
inline constexpr size_t ACCOUNT_CREATE_V2_SIZE{9334};

struct AccountCreationWorkV2 {
    uint256 network_id;
    AccountId account_id;
    std::array<unsigned char, 32> authorization_commitment{};
    uint64_t work_epoch{0};
    uint64_t nonce{0};
};

struct AccountCreateOpV2 {
    AccountId account_id;
    IdentityAuthorizationV2 authorization;
    AccountCreationWorkV2 work;
    IdentityHybridSignature recovery_pop;
    IdentityHybridSignature device_pop;
};

enum class AccountCreateV2Error : uint8_t {
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

std::optional<std::array<unsigned char, ACCOUNT_CREATE_V2_WORK_SIZE>> SerializeAccountCreationWorkV2(
    const AccountCreationWorkV2& work);
std::optional<std::array<unsigned char, ACCOUNT_CREATE_V2_SIZE>> SerializeAccountCreateOpV2(
    const AccountCreateOpV2& op);
std::optional<AccountCreateOpV2> DeserializeAccountCreateOpV2(std::span<const unsigned char> bytes);
std::optional<std::array<unsigned char, 32>> ComputeAccountCreateWorkHashV2(const AccountCreationWorkV2& work);
std::optional<std::array<unsigned char, 32>> ComputeAccountCreatePopDigestV2(
    const uint256& network_id, const AccountId& account_id,
    const IdentityAuthorizationV2& authorization);
AccountCreateV2Error ValidateAccountCreateOpV2(
    const AccountCreateOpV2& op, const uint256& network_id,
    uint64_t block_height, const CybouProtocolParameters& params);

inline constexpr size_t ACCOUNT_CREATE_WORK_SIZE{ACCOUNT_CREATE_V2_WORK_SIZE};
inline constexpr size_t ACCOUNT_CREATE_SIZE{ACCOUNT_CREATE_V2_SIZE};
using AccountCreationWork = AccountCreationWorkV2;
using AccountCreateOp = AccountCreateOpV2;
using AccountCreateError = AccountCreateV2Error;

inline std::optional<std::array<unsigned char, ACCOUNT_CREATE_WORK_SIZE>> SerializeAccountCreationWork(
    const AccountCreationWork& work)
{
    return SerializeAccountCreationWorkV2(work);
}

inline std::optional<std::array<unsigned char, ACCOUNT_CREATE_SIZE>> SerializeAccountCreateOp(
    const AccountCreateOp& op)
{
    return SerializeAccountCreateOpV2(op);
}

inline std::optional<AccountCreateOp> DeserializeAccountCreateOp(std::span<const unsigned char> bytes)
{
    return DeserializeAccountCreateOpV2(bytes);
}

inline std::optional<std::array<unsigned char, 32>> ComputeAccountCreateWorkHash(const AccountCreationWork& work)
{
    return ComputeAccountCreateWorkHashV2(work);
}

inline std::optional<std::array<unsigned char, 32>> ComputeAccountCreatePopDigest(
    const uint256& network_id, const AccountId& account_id,
    const IdentityAuthorization& authorization)
{
    return ComputeAccountCreatePopDigestV2(network_id, account_id, authorization);
}

inline AccountCreateError ValidateAccountCreateOp(
    const AccountCreateOp& op, const uint256& network_id,
    uint64_t block_height, const CybouProtocolParameters& params)
{
    return ValidateAccountCreateOpV2(op, network_id, block_height, params);
}

} // namespace cybou
#endif
