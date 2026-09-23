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

enum class AuthorizedPayloadType : uint8_t {
    PAYMENT = 1,
    KEY_UPDATE = 2,
};

struct PaymentOpV1 {
    uint8_t version{PAYMENT_OP_VERSION};
    AccountId recipient;
    uint64_t amount{0};
    uint64_t fee{0};

    friend bool operator==(const PaymentOpV1&, const PaymentOpV1&) = default;
};

struct KeyUpdateOpV1 {
    uint8_t version{KEY_UPDATE_OP_VERSION};
    AccountAuthorizationV1 new_authorization;

    friend bool operator==(const KeyUpdateOpV1&, const KeyUpdateOpV1&) = default;
};

using AuthorizedOperationPayloadV1 = std::variant<PaymentOpV1, KeyUpdateOpV1>;

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

enum class ProtocolOperationType : uint8_t {
    ACCOUNT_CREATE = 1,
    AUTHORIZED_OPERATION = 2,
};

using ProtocolOperationPayloadV1 = std::variant<AccountCreateOpV1, AuthorizedOperationV1>;

struct ProtocolOperationV1 {
    uint8_t version{PROTOCOL_OPERATION_VERSION};
    ProtocolOperationPayloadV1 payload;

    ProtocolOperationV1() = default;
    ProtocolOperationV1(AccountCreateOpV1 operation) : payload{std::move(operation)} {}
    ProtocolOperationV1(AuthorizedOperationV1 operation) : payload{std::move(operation)} {}

    friend bool operator==(const ProtocolOperationV1&, const ProtocolOperationV1&) = default;
};

ProtocolOperationType OperationType(const ProtocolOperationV1& operation);
std::vector<unsigned char> SerializeProtocolOperation(const ProtocolOperationV1& operation);
std::optional<ProtocolOperationV1> DeserializeProtocolOperation(std::span<const unsigned char> bytes);

struct ProtocolExecutionContextV1 {
    uint256 network_id;
    uint64_t block_height{0};
    const CybouProtocolParameters& params;
};

enum class OperationExecutionError : uint8_t {
    NONE,
    ACCOUNT_CREATE_FAILED,
    ACCOUNT_NOT_FOUND,
    BAD_NONCE,
    INVALID_SIGNATURE,
    PAYMENT_FAILED,
    KEY_UPDATE_FAILED,
};

struct OperationExecutionResult {
    OperationExecutionError error{OperationExecutionError::NONE};
    AccountCreateResult account_create_result{};
    PaymentResult payment_result{};
    KeyUpdateResult key_update_result{};

    explicit operator bool() const { return error == OperationExecutionError::NONE; }
};

OperationExecutionResult ApplyProtocolOperation(
    const ProtocolOperationV1& operation,
    const ProtocolExecutionContextV1& context,
    CybouState& state);

} // namespace cybou

#endif // CYBOU_PROTOCOL_OPERATION_H
