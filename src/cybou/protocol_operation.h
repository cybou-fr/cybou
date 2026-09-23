// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_OPERATION_H
#define CYBOU_PROTOCOL_OPERATION_H

#include <cybou/account_creation.h>
#include <cybou/protocol_params.h>
#include <cybou/signing.h>
#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace cybou {

inline constexpr uint8_t PROTOCOL_OPERATION_VERSION{1};
inline constexpr uint8_t AUTHORIZED_OPERATION_VERSION{1};
inline constexpr uint8_t PAYMENT_OP_VERSION{1};
inline constexpr uint8_t KEY_UPDATE_OP_VERSION{1};
inline constexpr uint8_t SYSTEM_LOCK_OP_VERSION{1};
inline constexpr uint8_t MAIL_OP_VERSION{1};
inline constexpr uint8_t VALIDATOR_ADMISSION_OP_VERSION{1};
inline constexpr uint8_t VALIDATOR_REMOVAL_OP_VERSION{1};

uint256 ComputeMailContentCommitment(const uint256& salt, std::span<const unsigned char> ciphertext);

enum class AuthorizedPayloadType : uint8_t {
    PAYMENT = 1,
    KEY_UPDATE = 2,
    SYSTEM_LOCK = 3,
    MAIL = 4,
};

struct PaymentOpV1 {
    uint8_t version{PAYMENT_OP_VERSION};
    AccountId recipient;
    uint64_t amount{0};

    friend bool operator==(const PaymentOpV1&, const PaymentOpV1&) = default;
};

struct KeyUpdateOpV1 {
    uint8_t version{KEY_UPDATE_OP_VERSION};
    AccountAuthorizationV1 new_authorization;

    friend bool operator==(const KeyUpdateOpV1&, const KeyUpdateOpV1&) = default;
};

struct SystemLockOpV1 {
    uint8_t version{SYSTEM_LOCK_OP_VERSION};
    uint64_t amount{0};

    friend bool operator==(const SystemLockOpV1&, const SystemLockOpV1&) = default;
};

/**
 * DEV EXPERIMENTAL — NOT WIRE-FROZEN
 * Bounded encrypted text mail operation. Fee is determined deterministically
 * by the protocol parameters at execution time, not specified by the sender.
 */
struct MailOpV1 {
    uint8_t version{MAIL_OP_VERSION};
    AccountId recipient;
    uint256 content_commitment;
    uint256 discovery_tag;
    std::vector<unsigned char> ciphertext;

    friend bool operator==(const MailOpV1&, const MailOpV1&) = default;
};

using AuthorizedOperationPayloadV1 = std::variant<PaymentOpV1, KeyUpdateOpV1, SystemLockOpV1, MailOpV1>;

AuthorizedPayloadType PayloadType(const AuthorizedOperationPayloadV1& payload);
std::vector<unsigned char> SerializeAuthorizedPayload(const AuthorizedOperationPayloadV1& payload);
std::optional<AuthorizedOperationPayloadV1> DeserializeAuthorizedPayload(std::span<const unsigned char> bytes);

struct AuthorizedOperationV1 {
    uint8_t version{AUTHORIZED_OPERATION_VERSION};
    AccountId account_id;
    uint64_t nonce{0};
    AuthorizedOperationPayloadV1 payload;
    std::array<unsigned char, USER_SIGNATURE_SIZE> signature{};

    friend bool operator==(const AuthorizedOperationV1&, const AuthorizedOperationV1&) = default;
};

std::vector<unsigned char> SerializeAuthorizedOperation(const AuthorizedOperationV1& op);
std::optional<AuthorizedOperationV1> DeserializeAuthorizedOperation(std::span<const unsigned char> bytes);

/** Compute domain-separated signing digest: SHA256("CYBOU/USER_OP/V1" || network_id || account_id || nonce || serialized(payload)) */
uint256 ComputeUserOperationDigest(
    const uint256& network_id,
    const AccountId& account_id,
    uint64_t nonce,
    const AuthorizedOperationPayloadV1& payload);

struct ValidatorAdmissionOpV1 {
    uint8_t version{VALIDATOR_ADMISSION_OP_VERSION};
    uint256 validator_id;
    uint256 consensus_public_key;
    uint64_t activation_epoch{0};
    SignatureBundleV1 operator_signature;

    friend bool operator==(const ValidatorAdmissionOpV1&, const ValidatorAdmissionOpV1&) = default;
};

std::vector<unsigned char> SerializeValidatorAdmissionOp(const ValidatorAdmissionOpV1& op);
std::optional<ValidatorAdmissionOpV1> DeserializeValidatorAdmissionOp(std::span<const unsigned char> bytes);
std::vector<unsigned char> ComputeValidatorAdmissionSigningData(
    const uint256& network_id,
    const ValidatorAdmissionOpV1& op);

struct ValidatorRemovalOpV1 {
    uint8_t version{VALIDATOR_REMOVAL_OP_VERSION};
    uint256 validator_id;
    uint64_t effective_epoch{0};
    SignatureBundleV1 operator_signature;

    friend bool operator==(const ValidatorRemovalOpV1&, const ValidatorRemovalOpV1&) = default;
};

std::vector<unsigned char> SerializeValidatorRemovalOp(const ValidatorRemovalOpV1& op);
std::optional<ValidatorRemovalOpV1> DeserializeValidatorRemovalOp(std::span<const unsigned char> bytes);
std::vector<unsigned char> ComputeValidatorRemovalSigningData(
    const uint256& network_id,
    const ValidatorRemovalOpV1& op);

enum class ProtocolOperationType : uint8_t {
    ACCOUNT_CREATE = 1,
    AUTHORIZED_OPERATION = 2,
    VALIDATOR_ADMISSION = 3,
    VALIDATOR_REMOVAL = 4,
};

using ProtocolOperationPayloadV1 = std::variant<
    AccountCreateOpV1,
    AuthorizedOperationV1,
    ValidatorAdmissionOpV1,
    ValidatorRemovalOpV1>;

struct ProtocolOperationV1 {
    uint8_t version{PROTOCOL_OPERATION_VERSION};
    ProtocolOperationPayloadV1 payload;

    ProtocolOperationV1() = default;
    ProtocolOperationV1(AccountCreateOpV1 operation) : payload{std::move(operation)} {}
    ProtocolOperationV1(AuthorizedOperationV1 operation) : payload{std::move(operation)} {}
    ProtocolOperationV1(ValidatorAdmissionOpV1 operation) : payload{std::move(operation)} {}
    ProtocolOperationV1(ValidatorRemovalOpV1 operation) : payload{std::move(operation)} {}

    friend bool operator==(const ProtocolOperationV1&, const ProtocolOperationV1&) = default;
};

ProtocolOperationType OperationType(const ProtocolOperationV1& operation);
std::vector<unsigned char> SerializeProtocolOperation(const ProtocolOperationV1& operation);
std::optional<ProtocolOperationV1> DeserializeProtocolOperation(std::span<const unsigned char> bytes);

struct ProtocolExecutionContextV1 {
    uint256 network_id;
    uint64_t block_height{0};
    const CybouProtocolParameters& params;
    const OperatorAuthorityKeySet* operator_authority{nullptr};
    const OperatorAuthoritySignatureVerifier* operator_verifier{nullptr};
};

enum class ValidatorAdmissionError : uint8_t {
    NONE,
    NULL_VALIDATOR_ID,
    NULL_CONSENSUS_KEY,
    ALREADY_EXISTS,
    DUPLICATE_CONSENSUS_KEY,
    FUTURE_EPOCH,
    OPERATOR_AUTHORITY_MISSING,
    OPERATOR_KEYSET_MISMATCH,
    OPERATOR_KEYSET_INACTIVE,
    INVALID_OPERATOR_SIGNATURE,
};

struct ValidatorAdmissionResult {
    ValidatorAdmissionError error{ValidatorAdmissionError::NONE};

    explicit operator bool() const { return error == ValidatorAdmissionError::NONE; }
};

ValidatorAdmissionResult ApplyValidatorAdmission(
    const ValidatorAdmissionOpV1& op,
    const ProtocolExecutionContextV1& context,
    CybouState& state);

enum class ValidatorRemovalError : uint8_t {
    NONE,
    NOT_FOUND,
    CANNOT_REMOVE_LAST_VALIDATOR,
    FUTURE_EPOCH,
    OPERATOR_AUTHORITY_MISSING,
    OPERATOR_KEYSET_MISMATCH,
    OPERATOR_KEYSET_INACTIVE,
    INVALID_OPERATOR_SIGNATURE,
};

struct ValidatorRemovalResult {
    ValidatorRemovalError error{ValidatorRemovalError::NONE};

    explicit operator bool() const { return error == ValidatorRemovalError::NONE; }
};

ValidatorRemovalResult ApplyValidatorRemoval(
    const ValidatorRemovalOpV1& op,
    const ProtocolExecutionContextV1& context,
    CybouState& state);

enum class OperationExecutionError : uint8_t {
    NONE,
    ACCOUNT_CREATE_FAILED,
    ACCOUNT_NOT_FOUND,
    BAD_NONCE,
    INVALID_SIGNATURE,
    PAYMENT_FAILED,
    KEY_UPDATE_FAILED,
    SYSTEM_LOCK_FAILED,
    MAIL_FAILED,
    MAIL_OVERSIZED,
    MAIL_RATE_LIMIT_EXCEEDED,
    VALIDATOR_ADMISSION_FAILED,
    VALIDATOR_REMOVAL_FAILED,
};

struct OperationExecutionResult {
    OperationExecutionError error{OperationExecutionError::NONE};
    AccountCreateResult account_create_result{};
    PaymentResult payment_result{};
    KeyUpdateResult key_update_result{};
    SystemLockResult system_lock_result{};
    MailResult mail_result{};
    ValidatorAdmissionResult validator_admission_result{};
    ValidatorRemovalResult validator_removal_result{};

    explicit operator bool() const { return error == OperationExecutionError::NONE; }
};

OperationExecutionResult ApplyProtocolOperation(
    const ProtocolOperationV1& operation,
    const ProtocolExecutionContextV1& context,
    CybouState& state);

} // namespace cybou

#endif // CYBOU_PROTOCOL_OPERATION_H
