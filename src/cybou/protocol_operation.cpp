// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/protocol_operation.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <limits>
#include <string_view>

namespace cybou {
namespace {

std::vector<unsigned char> SerializePaymentOp(const PaymentOpV1& op)
{
    std::vector<unsigned char> out;
    out.push_back(op.version);
    out.insert(out.end(), op.recipient.Value().begin(), op.recipient.Value().end());
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(op.amount >> (8 * i)));
    return out;
}

std::optional<PaymentOpV1> DeserializePaymentOp(const std::span<const unsigned char> bytes)
{
    if (bytes.size() != 1 + 32 + 8) return std::nullopt;
    if (bytes[0] != PAYMENT_OP_VERSION) return std::nullopt;
    uint256 rec_bytes;
    std::copy_n(bytes.begin() + 1, 32, rec_bytes.begin());
    const AccountId recipient{rec_bytes};
    if (recipient.IsNull()) return std::nullopt;
    uint64_t amount{0};
    for (int i = 0; i < 8; ++i) amount |= uint64_t{bytes[33 + i]} << (8 * i);
    return PaymentOpV1{
        .version = bytes[0],
        .recipient = recipient,
        .amount = amount,
    };
}

std::vector<unsigned char> SerializeKeyUpdateOp(const KeyUpdateOpV1& op)
{
    std::vector<unsigned char> out;
    out.push_back(op.version);
    const auto auth{SerializeAccountAuthorization(op.new_authorization)};
    out.insert(out.end(), auth.begin(), auth.end());
    return out;
}

std::optional<KeyUpdateOpV1> DeserializeKeyUpdateOp(const std::span<const unsigned char> bytes)
{
    if (bytes.size() != 1 + 32) return std::nullopt;
    if (bytes[0] != KEY_UPDATE_OP_VERSION) return std::nullopt;
    uint256 descriptor;
    std::copy_n(bytes.begin() + 1, 32, descriptor.begin());
    if (descriptor.IsNull()) return std::nullopt;
    return KeyUpdateOpV1{
        .version = bytes[0],
        .new_authorization = AccountAuthorizationV1{.authorization_descriptor = descriptor},
    };
}

std::vector<unsigned char> SerializeSystemLockOp(const SystemLockOpV1& op)
{
    std::vector<unsigned char> out;
    out.push_back(op.version);
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(op.amount >> (8 * i)));
    return out;
}

std::optional<SystemLockOpV1> DeserializeSystemLockOp(const std::span<const unsigned char> bytes)
{
    if (bytes.size() != 1 + 8) return std::nullopt;
    if (bytes[0] != SYSTEM_LOCK_OP_VERSION) return std::nullopt;
    uint64_t amount{0};
    for (int i = 0; i < 8; ++i) amount |= uint64_t{bytes[1 + i]} << (8 * i);
    return SystemLockOpV1{.version = bytes[0], .amount = amount};
}

std::vector<unsigned char> SerializeMailOp(const MailOpV1& op)
{
    std::vector<unsigned char> out;
    out.reserve(101 + op.ciphertext.size());
    out.push_back(op.version);
    out.insert(out.end(), op.recipient.Value().begin(), op.recipient.Value().end());
    out.insert(out.end(), op.content_commitment.begin(), op.content_commitment.end());
    out.insert(out.end(), op.discovery_tag.begin(), op.discovery_tag.end());
    const uint32_t size = static_cast<uint32_t>(op.ciphertext.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(size >> (8 * i)));
    out.insert(out.end(), op.ciphertext.begin(), op.ciphertext.end());
    return out;
}

std::optional<MailOpV1> DeserializeMailOp(const std::span<const unsigned char> bytes)
{
    static constexpr size_t HEADER_SIZE{1 + 32 + 32 + 32 + 4};
    if (bytes.size() < HEADER_SIZE) return std::nullopt;
    if (bytes[0] != MAIL_OP_VERSION) return std::nullopt;

    uint256 rec_bytes;
    std::copy_n(bytes.begin() + 1, 32, rec_bytes.begin());
    const AccountId recipient{rec_bytes};
    if (recipient.IsNull()) return std::nullopt;

    uint256 content_commitment;
    std::copy_n(bytes.begin() + 33, 32, content_commitment.begin());

    uint256 discovery_tag;
    std::copy_n(bytes.begin() + 65, 32, discovery_tag.begin());

    uint32_t size{0};
    for (int i = 0; i < 4; ++i) size |= uint32_t{bytes[97 + i]} << (8 * i);

    if (bytes.size() != HEADER_SIZE + size) return std::nullopt;

    std::vector<unsigned char> ciphertext(bytes.begin() + HEADER_SIZE, bytes.end());
    return MailOpV1{
        .version = bytes[0],
        .recipient = recipient,
        .content_commitment = content_commitment,
        .discovery_tag = discovery_tag,
        .ciphertext = std::move(ciphertext),
    };
}

} // namespace

uint256 ComputeMailContentCommitment(const uint256& salt, const std::span<const unsigned char> ciphertext)
{
    static constexpr std::string_view DOMAIN{"CYBOU/MAIL_CONTENT/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(salt.begin(), salt.size());
    hasher.Write(ciphertext.data(), ciphertext.size());
    uint256 commitment;
    hasher.Finalize(commitment.begin());
    return commitment;
}

AuthorizedPayloadType PayloadType(const AuthorizedOperationPayloadV1& payload)
{
    return std::visit([](const auto& op) -> AuthorizedPayloadType {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, PaymentOpV1>) return AuthorizedPayloadType::PAYMENT;
        if constexpr (std::is_same_v<T, KeyUpdateOpV1>) return AuthorizedPayloadType::KEY_UPDATE;
        if constexpr (std::is_same_v<T, SystemLockOpV1>) return AuthorizedPayloadType::SYSTEM_LOCK;
        if constexpr (std::is_same_v<T, MailOpV1>) return AuthorizedPayloadType::MAIL;
    }, payload);
}

std::vector<unsigned char> SerializeAuthorizedPayload(const AuthorizedOperationPayloadV1& payload)
{
    std::vector<unsigned char> out;
    out.push_back(static_cast<uint8_t>(PayloadType(payload)));
    const std::vector<unsigned char> body{std::visit([](const auto& op) {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, PaymentOpV1>) return SerializePaymentOp(op);
        if constexpr (std::is_same_v<T, KeyUpdateOpV1>) return SerializeKeyUpdateOp(op);
        if constexpr (std::is_same_v<T, SystemLockOpV1>) return SerializeSystemLockOp(op);
        if constexpr (std::is_same_v<T, MailOpV1>) return SerializeMailOp(op);
    }, payload)};
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

std::optional<AuthorizedOperationPayloadV1> DeserializeAuthorizedPayload(const std::span<const unsigned char> bytes)
{
    if (bytes.size() < 2) return std::nullopt;
    const auto type{static_cast<AuthorizedPayloadType>(bytes[0])};
    const auto sub{bytes.subspan(1)};
    switch (type) {
    case AuthorizedPayloadType::PAYMENT: {
        const auto op{DeserializePaymentOp(sub)};
        if (!op) return std::nullopt;
        return *op;
    }
    case AuthorizedPayloadType::KEY_UPDATE: {
        const auto op{DeserializeKeyUpdateOp(sub)};
        if (!op) return std::nullopt;
        return *op;
    }
    case AuthorizedPayloadType::SYSTEM_LOCK: {
        const auto op{DeserializeSystemLockOp(sub)};
        if (!op) return std::nullopt;
        return *op;
    }
    case AuthorizedPayloadType::MAIL: {
        const auto op{DeserializeMailOp(sub)};
        if (!op) return std::nullopt;
        return *op;
    }
    }
    return std::nullopt;
}

std::vector<unsigned char> SerializeAuthorizedOperation(const AuthorizedOperationV1& op)
{
    std::vector<unsigned char> out;
    out.push_back(op.version);
    out.insert(out.end(), op.account_id.Value().begin(), op.account_id.Value().end());
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(op.nonce >> (8 * i)));
    out.insert(out.end(), op.signature.begin(), op.signature.end());
    const auto payload_bytes{SerializeAuthorizedPayload(op.payload)};
    out.insert(out.end(), payload_bytes.begin(), payload_bytes.end());
    return out;
}

std::optional<AuthorizedOperationV1> DeserializeAuthorizedOperation(const std::span<const unsigned char> bytes)
{
    static constexpr size_t HEADER_SIZE{1 + 32 + 8 + USER_SIGNATURE_SIZE};
    if (bytes.size() < HEADER_SIZE + 2) return std::nullopt;
    if (bytes[0] != AUTHORIZED_OPERATION_VERSION) return std::nullopt;
    uint256 acc_bytes;
    std::copy_n(bytes.begin() + 1, 32, acc_bytes.begin());
    const AccountId account_id{acc_bytes};
    if (account_id.IsNull()) return std::nullopt;
    uint64_t nonce{0};
    for (int i = 0; i < 8; ++i) nonce |= uint64_t{bytes[33 + i]} << (8 * i);
    std::array<unsigned char, USER_SIGNATURE_SIZE> sig{};
    std::copy_n(bytes.begin() + 41, USER_SIGNATURE_SIZE, sig.begin());
    const auto payload{DeserializeAuthorizedPayload(bytes.subspan(HEADER_SIZE))};
    if (!payload) return std::nullopt;
    return AuthorizedOperationV1{
        .version = bytes[0],
        .account_id = account_id,
        .nonce = nonce,
        .payload = *payload,
        .signature = sig,
    };
}

uint256 ComputeUserOperationDigest(
    const uint256& network_id,
    const AccountId& account_id,
    const uint64_t nonce,
    const AuthorizedOperationPayloadV1& payload)
{
    static constexpr std::string_view DOMAIN{"CYBOU/USER_OP/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());
    hasher.Write(account_id.Value().begin(), account_id.Value().size());
    std::array<unsigned char, 8> nonce_bytes{};
    for (int i = 0; i < 8; ++i) nonce_bytes[i] = static_cast<unsigned char>(nonce >> (8 * i));
    hasher.Write(nonce_bytes.data(), nonce_bytes.size());
    const auto payload_bytes{SerializeAuthorizedPayload(payload)};
    hasher.Write(payload_bytes.data(), payload_bytes.size());
    uint256 digest;
    hasher.Finalize(digest.begin());
    return digest;
}

ProtocolOperationType OperationType(const ProtocolOperationV1& operation)
{
    return std::visit([](const auto& op) -> ProtocolOperationType {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, AccountCreateOpV1>) return ProtocolOperationType::ACCOUNT_CREATE;
        if constexpr (std::is_same_v<T, AuthorizedOperationV1>) return ProtocolOperationType::AUTHORIZED_OPERATION;
    }, operation.payload);
}

std::vector<unsigned char> SerializeProtocolOperation(const ProtocolOperationV1& operation)
{
    std::vector<unsigned char> out{operation.version, static_cast<uint8_t>(OperationType(operation))};
    const auto payload{std::visit([](const auto& value) -> std::vector<unsigned char> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AccountCreateOpV1>) return SerializeAccountCreateOp(value);
        if constexpr (std::is_same_v<T, AuthorizedOperationV1>) return SerializeAuthorizedOperation(value);
    }, operation.payload)};
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::optional<ProtocolOperationV1> DeserializeProtocolOperation(const std::span<const unsigned char> bytes)
{
    if (bytes.size() < 2 || bytes[0] != PROTOCOL_OPERATION_VERSION) return std::nullopt;
    const auto type{static_cast<ProtocolOperationType>(bytes[1])};
    const auto payload_bytes{bytes.subspan(2)};
    switch (type) {
    case ProtocolOperationType::ACCOUNT_CREATE: {
        const auto account_create{DeserializeAccountCreateOp(payload_bytes)};
        if (!account_create) return std::nullopt;
        return ProtocolOperationV1{*account_create};
    }
    case ProtocolOperationType::AUTHORIZED_OPERATION: {
        const auto auth_op{DeserializeAuthorizedOperation(payload_bytes)};
        if (!auth_op) return std::nullopt;
        return ProtocolOperationV1{*auth_op};
    }
    }
    return std::nullopt;
}

OperationExecutionResult ApplyProtocolOperation(
    const ProtocolOperationV1& operation,
    const ProtocolExecutionContextV1& context,
    CybouState& state)
{
    return std::visit([&](const auto& op) -> OperationExecutionResult {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, AccountCreateOpV1>) {
            const auto res{ApplyAccountCreate(op, context.network_id, context.block_height, context.params, state)};
            if (!res) {
                return {OperationExecutionError::ACCOUNT_CREATE_FAILED, res, {}, {}};
            }
            return {OperationExecutionError::NONE, res, {}, {}};
        }
        if constexpr (std::is_same_v<T, AuthorizedOperationV1>) {
            auto account_it{state.accounts.find(op.account_id)};
            if (account_it == state.accounts.end()) {
                return {OperationExecutionError::ACCOUNT_NOT_FOUND, {}, {}, {}};
            }
            if (op.nonce != account_it->second.next_nonce) {
                return {OperationExecutionError::BAD_NONCE, {}, {}, {}};
            }
            const uint256 digest{ComputeUserOperationDigest(context.network_id, op.account_id, op.nonce, op.payload)};
            if (!VerifyUserSignature(
                    account_it->second.active_authorization_key,
                    op.signature,
                    std::span<const unsigned char>{digest.begin(), digest.size()})) {
                return {OperationExecutionError::INVALID_SIGNATURE, {}, {}, {}};
            }

            return std::visit([&](const auto& payload) -> OperationExecutionResult {
                using P = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<P, PaymentOpV1>) {
                    const auto payment_res{ApplyPayment(op.account_id, payload.recipient, payload.amount, context.params.payment_fee, state)};
                    if (!payment_res) {
                        return {OperationExecutionError::PAYMENT_FAILED, {}, payment_res, {}, {}, {}};
                    }
                    return {OperationExecutionError::NONE, {}, payment_res, {}, {}, {}};
                }
                if constexpr (std::is_same_v<P, KeyUpdateOpV1>) {
                    const auto key_res{ApplyKeyUpdate(op.account_id, payload.new_authorization.authorization_descriptor, state)};
                    if (!key_res) {
                        return {OperationExecutionError::KEY_UPDATE_FAILED, {}, {}, key_res, {}, {}};
                    }
                    return {OperationExecutionError::NONE, {}, {}, key_res, {}, {}};
                }
                if constexpr (std::is_same_v<P, SystemLockOpV1>) {
                    const auto lock_res{ApplySystemLock(op.account_id, payload.amount, state)};
                    if (!lock_res) {
                        return {OperationExecutionError::SYSTEM_LOCK_FAILED, {}, {}, {}, lock_res, {}};
                    }
                    return {OperationExecutionError::NONE, {}, {}, {}, lock_res, {}};
                }
                if constexpr (std::is_same_v<P, MailOpV1>) {
                    if (payload.ciphertext.size() > context.params.max_mail_ciphertext_size) {
                        return {OperationExecutionError::MAIL_OVERSIZED, {}, {}, {}, {}, {}};
                    }
                    const uint64_t fee{MailFeeForSize(payload.ciphertext.size(), context.params)};
                    const auto mail_res{ApplyMail(op.account_id, payload.recipient, fee, state)};
                    if (!mail_res) {
                        return {OperationExecutionError::MAIL_FAILED, {}, {}, {}, {}, mail_res};
                    }
                    return {OperationExecutionError::NONE, {}, {}, {}, {}, mail_res};
                }
            }, op.payload);
        }
    }, operation.payload);
}

} // namespace cybou
