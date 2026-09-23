// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ACCOUNT_CREATION_H
#define CYBOU_ACCOUNT_CREATION_H

#include <cybou/account_id.h>
#include <cybou/identity.h>
#include <cybou/protocol_params.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t ACCOUNT_CREATION_WORK_VERSION{1};
inline constexpr uint8_t ACCOUNT_CREATE_OP_VERSION{1};
inline constexpr size_t ACCOUNT_CREATION_WORK_SERIALIZED_SIZE{113};

/**
 * Anti-Sybil Proof-of-Work for permissionless account creation.
 * Binds the account ID and initial authorization to work spent.
 */
struct AccountCreationWorkV1 {
    uint8_t version{ACCOUNT_CREATION_WORK_VERSION};
    uint256 network_id;
    AccountId account_id;
    uint256 initial_authorization_commitment;
    uint64_t work_epoch{0};
    uint64_t nonce{0};

    friend bool operator==(const AccountCreationWorkV1&, const AccountCreationWorkV1&) = default;
};

inline constexpr size_t ACCOUNT_POP_SIGNATURE_SIZE{64};

/**
 * Protocol-native account creation operation.
 */
struct AccountCreateOpV1 {
    uint8_t version{ACCOUNT_CREATE_OP_VERSION};
    AccountId account_id;
    AccountAuthorizationV1 initial_authorization;
    AccountCreationWorkV1 creation_work;
    std::array<unsigned char, ACCOUNT_POP_SIGNATURE_SIZE> proof_of_possession{};

    friend bool operator==(const AccountCreateOpV1&, const AccountCreateOpV1&) = default;
};

std::vector<unsigned char> SerializeAccountCreationWork(const AccountCreationWorkV1& work);
std::optional<AccountCreationWorkV1> DeserializeAccountCreationWork(std::span<const unsigned char> bytes);

uint256 ComputeAccountCreationWorkHash(const AccountCreationWorkV1& work);
unsigned int CountLeadingZeroBits(const uint256& hash);

/** Check whether work meets difficulty requirement (minimum leading zero bits). */
bool CheckAccountCreationWork(const AccountCreationWorkV1& work, unsigned int required_leading_zero_bits);

/** Compute domain-separated digest for AccountCreate proof of possession: SHA256("CYBOU/ACCOUNT_POP/V1" || network_id || account_id || pubkey) */
uint256 ComputeAccountPopDigest(
    const uint256& network_id,
    const AccountId& account_id,
    const uint256& authorization_key);

std::vector<unsigned char> SerializeAccountCreateOp(const AccountCreateOpV1& op);
std::optional<AccountCreateOpV1> DeserializeAccountCreateOp(std::span<const unsigned char> bytes);

enum class AccountCreateValidationError : uint8_t {
    NONE,
    UNSUPPORTED_VERSION,
    NULL_ACCOUNT_ID,
    NULL_NETWORK_ID,
    NETWORK_MISMATCH,
    AUTH_COMMITMENT_MISMATCH,
    ACCOUNT_ID_MISMATCH,
    FUTURE_WORK_EPOCH,
    EXPIRED_WORK_EPOCH,
    INSUFFICIENT_WORK,
    INVALID_PROOF_OF_POSSESSION,
};

/**
 * Validate an AccountCreate operation at a given finalized block height.
 * The PoT epoch is derived internally from the height via EpochForHeight();
 * callers must never supply an epoch directly.
 */
AccountCreateValidationError ValidateAccountCreateOp(
    const AccountCreateOpV1& op,
    const uint256& expected_network_id,
    uint64_t block_height,
    const CybouProtocolParameters& params);

} // namespace cybou

#endif // CYBOU_ACCOUNT_CREATION_H
