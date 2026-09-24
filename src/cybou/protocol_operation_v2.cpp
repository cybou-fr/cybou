// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/protocol_operation_v2.h>

#include <openssl/evp.h>

#include <algorithm>
#include <memory>
#include <string_view>

namespace cybou {
namespace {
constexpr size_t DEVICE_AUTH_WIRE_SIZE{32 + 32 + 8 + 8 + 1 + 32 + 64 + 2420};

void Write64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint64_t Read64(std::span<const unsigned char> bytes)
{
    uint64_t value{0};
    for (unsigned i{0}; i < 8; ++i) value |= uint64_t{bytes[i]} << (8 * i);
    return value;
}

bool IsAllZero(std::span<const unsigned char> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](unsigned char b) { return b == 0; });
}

bool SerializeDeviceAuthorization(const DeviceAuthorizationV2& auth, DeviceOperationKindV2 expected_kind, std::vector<unsigned char>& out)
{
    if (auth.kind != expected_kind || auth.account_id.IsNull() || auth.signature.ml_dsa.size() != 2420 ||
        IsAllZero(auth.device_id) || IsAllZero(auth.payload_commitment) ||
        IsAllZero(auth.signature.ed25519) || IsAllZero(auth.signature.ml_dsa)) return false;
    out.insert(out.end(), auth.account_id.Value().begin(), auth.account_id.Value().end());
    out.insert(out.end(), auth.device_id.begin(), auth.device_id.end());
    Write64(out, auth.nonce);
    Write64(out, auth.activation_nonce);
    out.push_back(static_cast<unsigned char>(auth.kind));
    out.insert(out.end(), auth.payload_commitment.begin(), auth.payload_commitment.end());
    out.insert(out.end(), auth.signature.ed25519.begin(), auth.signature.ed25519.end());
    out.insert(out.end(), auth.signature.ml_dsa.begin(), auth.signature.ml_dsa.end());
    return true;
}

std::optional<DeviceAuthorizationV2> DeserializeDeviceAuthorization(std::span<const unsigned char> bytes, DeviceOperationKindV2 expected_kind)
{
    if (bytes.size() != DEVICE_AUTH_WIRE_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.first(32));
    if (!account) return std::nullopt;
    DeviceAuthorizationV2 auth{};
    auth.account_id = *account;
    size_t offset{32};
    std::copy_n(bytes.begin() + offset, 32, auth.device_id.begin());
    offset += 32;
    auth.nonce = Read64(bytes.subspan(offset, 8));
    offset += 8;
    auth.activation_nonce = Read64(bytes.subspan(offset, 8));
    offset += 8;
    if (bytes[offset++] != static_cast<uint8_t>(expected_kind)) return std::nullopt;
    auth.kind = expected_kind;
    std::copy_n(bytes.begin() + offset, 32, auth.payload_commitment.begin());
    offset += 32;
    std::copy_n(bytes.begin() + offset, 64, auth.signature.ed25519.begin());
    offset += 64;
    auth.signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 2420);
    offset += 2420;
    if (IsAllZero(auth.device_id) || IsAllZero(auth.payload_commitment) ||
        IsAllZero(auth.signature.ed25519) || IsAllZero(auth.signature.ml_dsa)) return std::nullopt;
    return auth;
}

std::optional<std::vector<unsigned char>> SerializePayment(const AuthorizedPaymentV2& operation)
{
    const auto payment = SerializePaymentPayloadV2(operation.payment);
    if (!payment) return std::nullopt;
    const auto commitment = ComputePaymentPayloadCommitmentV2(operation.payment);
    if (!commitment || *commitment != operation.authorization.payload_commitment) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(AUTHORIZED_PAYMENT_V2_SIZE);
    if (!SerializeDeviceAuthorization(operation.authorization, DeviceOperationKindV2::PAYMENT, out)) return std::nullopt;
    out.insert(out.end(), payment->begin(), payment->end());
    if (out.size() != AUTHORIZED_PAYMENT_V2_SIZE) return std::nullopt;
    return out;
}

std::optional<AuthorizedPaymentV2> DeserializePayment(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_PAYMENT_V2_SIZE) return std::nullopt;
    const auto auth = DeserializeDeviceAuthorization(bytes.first(DEVICE_AUTH_WIRE_SIZE), DeviceOperationKindV2::PAYMENT);
    if (!auth) return std::nullopt;
    const auto payment = DeserializePaymentPayloadV2(bytes.subspan(DEVICE_AUTH_WIRE_SIZE));
    if (!payment || ComputePaymentPayloadCommitmentV2(*payment) != auth->payload_commitment) return std::nullopt;
    return AuthorizedPaymentV2{.authorization = *auth, .payment = *payment};
}

std::optional<std::vector<unsigned char>> SerializeSystemLock(const AuthorizedSystemLockV2& operation)
{
    const auto lock = SerializeSystemLockPayloadV2(operation.lock);
    if (!lock) return std::nullopt;
    const auto commitment = ComputeSystemLockPayloadCommitmentV2(operation.lock);
    if (!commitment || *commitment != operation.authorization.payload_commitment) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(AUTHORIZED_SYSTEM_LOCK_V2_SIZE);
    if (!SerializeDeviceAuthorization(operation.authorization, DeviceOperationKindV2::SYSTEM_LOCK, out)) return std::nullopt;
    out.insert(out.end(), lock->begin(), lock->end());
    if (out.size() != AUTHORIZED_SYSTEM_LOCK_V2_SIZE) return std::nullopt;
    return out;
}

std::optional<AuthorizedSystemLockV2> DeserializeSystemLock(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_SYSTEM_LOCK_V2_SIZE) return std::nullopt;
    const auto auth = DeserializeDeviceAuthorization(bytes.first(DEVICE_AUTH_WIRE_SIZE), DeviceOperationKindV2::SYSTEM_LOCK);
    if (!auth) return std::nullopt;
    const auto lock = DeserializeSystemLockPayloadV2(bytes.subspan(DEVICE_AUTH_WIRE_SIZE));
    if (!lock || ComputeSystemLockPayloadCommitmentV2(*lock) != auth->payload_commitment) return std::nullopt;
    return AuthorizedSystemLockV2{.authorization = *auth, .lock = *lock};
}

std::optional<std::vector<unsigned char>> SerializeNameCommit(const AuthorizedNameCommit& op)
{
    const auto commit = SerializeNameCommitPayload(op.commit);
    if (!commit) return std::nullopt;
    const auto commitment = ComputeNameCommitPayloadCommitment(op.commit);
    if (!commitment || *commitment != op.authorization.payload_commitment) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(AUTHORIZED_NAME_COMMIT_V2_SIZE);
    if (!SerializeDeviceAuthorization(op.authorization, DeviceOperationKindV2::NAME_COMMIT, out)) return std::nullopt;
    out.insert(out.end(), commit->begin(), commit->end());
    if (out.size() != AUTHORIZED_NAME_COMMIT_V2_SIZE) return std::nullopt;
    return out;
}

std::optional<AuthorizedNameCommit> DeserializeNameCommit(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_NAME_COMMIT_V2_SIZE) return std::nullopt;
    const auto auth = DeserializeDeviceAuthorization(bytes.first(DEVICE_AUTH_WIRE_SIZE), DeviceOperationKindV2::NAME_COMMIT);
    if (!auth) return std::nullopt;
    const auto commit = DeserializeNameCommitPayload(bytes.subspan(DEVICE_AUTH_WIRE_SIZE));
    if (!commit || ComputeNameCommitPayloadCommitment(*commit) != auth->payload_commitment) return std::nullopt;
    return AuthorizedNameCommit{.authorization = *auth, .commit = *commit};
}

std::optional<std::vector<unsigned char>> SerializeNameReveal(const AuthorizedNameReveal& op)
{
    const auto reveal = SerializeNameRevealPayload(op.reveal);
    if (!reveal) return std::nullopt;
    const auto commitment = ComputeNameRevealPayloadCommitment(op.reveal);
    if (!commitment || *commitment != op.authorization.payload_commitment) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(AUTHORIZED_NAME_REVEAL_V2_SIZE);
    if (!SerializeDeviceAuthorization(op.authorization, DeviceOperationKindV2::NAME_REVEAL, out)) return std::nullopt;
    out.insert(out.end(), reveal->begin(), reveal->end());
    if (out.size() != AUTHORIZED_NAME_REVEAL_V2_SIZE) return std::nullopt;
    return out;
}

std::optional<AuthorizedNameReveal> DeserializeNameReveal(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_NAME_REVEAL_V2_SIZE) return std::nullopt;
    const auto auth = DeserializeDeviceAuthorization(bytes.first(DEVICE_AUTH_WIRE_SIZE), DeviceOperationKindV2::NAME_REVEAL);
    if (!auth) return std::nullopt;
    const auto reveal = DeserializeNameRevealPayload(bytes.subspan(DEVICE_AUTH_WIRE_SIZE));
    if (!reveal || ComputeNameRevealPayloadCommitment(*reveal) != auth->payload_commitment) return std::nullopt;
    return AuthorizedNameReveal{.authorization = *auth, .reveal = *reveal};
}

std::optional<std::vector<unsigned char>> SerializeDeviceAdd(const DeviceAddV2& op)
{
    if (op.account_id.IsNull() || op.new_device.purpose != IdentityKeyPurpose::DEVICE ||
        op.new_device.ml_dsa.size() != 1312 || op.root_signature.ml_dsa.size() != 3309 || op.device_pop.ml_dsa.size() != 2420 ||
        IsAllZero(op.new_device.ed25519) || IsAllZero(op.new_device.ml_dsa) ||
        IsAllZero(op.root_signature.ed25519) || IsAllZero(op.root_signature.ml_dsa) ||
        IsAllZero(op.device_pop.ed25519) || IsAllZero(op.device_pop.ml_dsa)) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(DEVICE_ADD_V2_SIZE);
    out.insert(out.end(), op.account_id.Value().begin(), op.account_id.Value().end());
    out.insert(out.end(), op.new_device.ed25519.begin(), op.new_device.ed25519.end());
    out.insert(out.end(), op.new_device.ml_dsa.begin(), op.new_device.ml_dsa.end());
    Write64(out, op.root_nonce);
    out.insert(out.end(), op.root_signature.ed25519.begin(), op.root_signature.ed25519.end());
    out.insert(out.end(), op.root_signature.ml_dsa.begin(), op.root_signature.ml_dsa.end());
    out.insert(out.end(), op.device_pop.ed25519.begin(), op.device_pop.ed25519.end());
    out.insert(out.end(), op.device_pop.ml_dsa.begin(), op.device_pop.ml_dsa.end());
    if (out.size() != DEVICE_ADD_V2_SIZE) return std::nullopt;
    return out;
}

std::optional<DeviceAddV2> DeserializeDeviceAdd(std::span<const unsigned char> bytes)
{
    if (bytes.size() != DEVICE_ADD_V2_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.first(32));
    if (!account) return std::nullopt;
    DeviceAddV2 op{};
    op.account_id = *account;
    op.new_device.purpose = IdentityKeyPurpose::DEVICE;
    size_t offset{32};
    std::copy_n(bytes.begin() + offset, 32, op.new_device.ed25519.begin());
    offset += 32;
    op.new_device.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 1312);
    offset += 1312;
    op.root_nonce = Read64(bytes.subspan(offset, 8));
    offset += 8;
    std::copy_n(bytes.begin() + offset, 64, op.root_signature.ed25519.begin());
    offset += 64;
    op.root_signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 3309);
    offset += 3309;
    std::copy_n(bytes.begin() + offset, 64, op.device_pop.ed25519.begin());
    offset += 64;
    op.device_pop.ml_dsa.assign(bytes.begin() + offset, bytes.end());
    if (IsAllZero(op.new_device.ed25519) || IsAllZero(op.new_device.ml_dsa) ||
        IsAllZero(op.root_signature.ed25519) || IsAllZero(op.root_signature.ml_dsa) ||
        IsAllZero(op.device_pop.ed25519) || IsAllZero(op.device_pop.ml_dsa)) return std::nullopt;
    return op;
}

std::optional<std::vector<unsigned char>> SerializeDeviceRevoke(const DeviceRevokeV2& op)
{
    if (op.account_id.IsNull() || op.root_signature.ml_dsa.size() != 3309 ||
        IsAllZero(op.device_id) || IsAllZero(op.root_signature.ed25519) || IsAllZero(op.root_signature.ml_dsa)) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(DEVICE_REVOKE_V2_SIZE);
    out.insert(out.end(), op.account_id.Value().begin(), op.account_id.Value().end());
    out.insert(out.end(), op.device_id.begin(), op.device_id.end());
    Write64(out, op.root_nonce);
    out.insert(out.end(), op.root_signature.ed25519.begin(), op.root_signature.ed25519.end());
    out.insert(out.end(), op.root_signature.ml_dsa.begin(), op.root_signature.ml_dsa.end());
    if (out.size() != DEVICE_REVOKE_V2_SIZE) return std::nullopt;
    return out;
}

std::optional<DeviceRevokeV2> DeserializeDeviceRevoke(std::span<const unsigned char> bytes)
{
    if (bytes.size() != DEVICE_REVOKE_V2_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.first(32));
    if (!account) return std::nullopt;
    DeviceRevokeV2 op{};
    op.account_id = *account;
    size_t offset{32};
    std::copy_n(bytes.begin() + offset, 32, op.device_id.begin());
    offset += 32;
    op.root_nonce = Read64(bytes.subspan(offset, 8));
    offset += 8;
    std::copy_n(bytes.begin() + offset, 64, op.root_signature.ed25519.begin());
    offset += 64;
    op.root_signature.ml_dsa.assign(bytes.begin() + offset, bytes.end());
    if (IsAllZero(op.device_id) || IsAllZero(op.root_signature.ed25519) || IsAllZero(op.root_signature.ml_dsa)) return std::nullopt;
    return op;
}

std::optional<std::vector<unsigned char>> SerializeRecoveryRotate(const RecoveryRotateV2& op)
{
    if (op.account_id.IsNull() || op.new_root.purpose != IdentityKeyPurpose::RECOVERY_ROOT ||
        op.new_root.ml_dsa.size() != 1952 || op.old_root_signature.ml_dsa.size() != 3309 || op.new_root_pop.ml_dsa.size() != 3309 ||
        IsAllZero(op.new_root.ed25519) || IsAllZero(op.new_root.ml_dsa) ||
        IsAllZero(op.old_root_signature.ed25519) || IsAllZero(op.old_root_signature.ml_dsa) ||
        IsAllZero(op.new_root_pop.ed25519) || IsAllZero(op.new_root_pop.ml_dsa)) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(RECOVERY_ROTATE_V2_SIZE);
    out.insert(out.end(), op.account_id.Value().begin(), op.account_id.Value().end());
    out.insert(out.end(), op.new_root.ed25519.begin(), op.new_root.ed25519.end());
    out.insert(out.end(), op.new_root.ml_dsa.begin(), op.new_root.ml_dsa.end());
    Write64(out, op.root_nonce);
    out.insert(out.end(), op.old_root_signature.ed25519.begin(), op.old_root_signature.ed25519.end());
    out.insert(out.end(), op.old_root_signature.ml_dsa.begin(), op.old_root_signature.ml_dsa.end());
    out.insert(out.end(), op.new_root_pop.ed25519.begin(), op.new_root_pop.ed25519.end());
    out.insert(out.end(), op.new_root_pop.ml_dsa.begin(), op.new_root_pop.ml_dsa.end());
    if (out.size() != RECOVERY_ROTATE_V2_SIZE) return std::nullopt;
    return out;
}

std::optional<RecoveryRotateV2> DeserializeRecoveryRotate(std::span<const unsigned char> bytes)
{
    if (bytes.size() != RECOVERY_ROTATE_V2_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.first(32));
    if (!account) return std::nullopt;
    RecoveryRotateV2 op{};
    op.account_id = *account;
    op.new_root.purpose = IdentityKeyPurpose::RECOVERY_ROOT;
    size_t offset{32};
    std::copy_n(bytes.begin() + offset, 32, op.new_root.ed25519.begin());
    offset += 32;
    op.new_root.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 1952);
    offset += 1952;
    op.root_nonce = Read64(bytes.subspan(offset, 8));
    offset += 8;
    std::copy_n(bytes.begin() + offset, 64, op.old_root_signature.ed25519.begin());
    offset += 64;
    op.old_root_signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 3309);
    offset += 3309;
    std::copy_n(bytes.begin() + offset, 64, op.new_root_pop.ed25519.begin());
    offset += 64;
    op.new_root_pop.ml_dsa.assign(bytes.begin() + offset, bytes.end());
    if (IsAllZero(op.new_root.ed25519) || IsAllZero(op.new_root.ml_dsa) ||
        IsAllZero(op.old_root_signature.ed25519) || IsAllZero(op.old_root_signature.ml_dsa) ||
        IsAllZero(op.new_root_pop.ed25519) || IsAllZero(op.new_root_pop.ml_dsa)) return std::nullopt;
    return op;
}
} // namespace

std::optional<std::vector<unsigned char>> SerializeProtocolOperationV2(const ProtocolOperationV2& operation)
{
    std::vector<unsigned char> out{PROTOCOL_OPERATION_VERSION_V2};
    if (const auto* create = std::get_if<AccountCreateOpV2>(&operation)) {
        const auto body = SerializeAccountCreateOpV2(*create);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::ACCOUNT_CREATE));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* payment = std::get_if<AuthorizedPaymentV2>(&operation)) {
        const auto body = SerializePayment(*payment);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::PAYMENT));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* add = std::get_if<DeviceAddV2>(&operation)) {
        const auto body = SerializeDeviceAdd(*add);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::DEVICE_ADD));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* revoke = std::get_if<DeviceRevokeV2>(&operation)) {
        const auto body = SerializeDeviceRevoke(*revoke);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::DEVICE_REVOKE));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* rotate = std::get_if<RecoveryRotateV2>(&operation)) {
        const auto body = SerializeRecoveryRotate(*rotate);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::RECOVERY_ROTATE));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* lock = std::get_if<AuthorizedSystemLockV2>(&operation)) {
        const auto body = SerializeSystemLock(*lock);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::SYSTEM_LOCK));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* commit = std::get_if<AuthorizedNameCommit>(&operation)) {
        const auto body = SerializeNameCommit(*commit);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::NAME_COMMIT));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* reveal = std::get_if<AuthorizedNameReveal>(&operation)) {
        const auto body = SerializeNameReveal(*reveal);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::NAME_REVEAL));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* mail = std::get_if<AuthorizedMail>(&operation)) {
        const auto body = SerializeAuthorizedMail(*mail);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::MAIL));
        out.insert(out.end(), body->begin(), body->end());
    } else {
        return std::nullopt;
    }
    return out;
}

std::optional<ProtocolOperationV2> DeserializeProtocolOperationV2(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 2 || bytes[0] != PROTOCOL_OPERATION_VERSION_V2) return std::nullopt;
    const auto kind = static_cast<ProtocolOperationKindV2>(bytes[1]);
    switch (kind) {
    case ProtocolOperationKindV2::ACCOUNT_CREATE: {
        if (bytes.size() != 2 + ACCOUNT_CREATE_V2_SIZE) return std::nullopt;
        const auto create = DeserializeAccountCreateOpV2(bytes.subspan(2));
        if (!create) return std::nullopt;
        return ProtocolOperationV2{*create};
    }
    case ProtocolOperationKindV2::PAYMENT: {
        if (bytes.size() != 2 + AUTHORIZED_PAYMENT_V2_SIZE) return std::nullopt;
        const auto payment = DeserializePayment(bytes.subspan(2));
        if (!payment) return std::nullopt;
        return ProtocolOperationV2{*payment};
    }
    case ProtocolOperationKindV2::DEVICE_ADD: {
        if (bytes.size() != 2 + DEVICE_ADD_V2_SIZE) return std::nullopt;
        const auto add = DeserializeDeviceAdd(bytes.subspan(2));
        if (!add) return std::nullopt;
        return ProtocolOperationV2{*add};
    }
    case ProtocolOperationKindV2::DEVICE_REVOKE: {
        if (bytes.size() != 2 + DEVICE_REVOKE_V2_SIZE) return std::nullopt;
        const auto revoke = DeserializeDeviceRevoke(bytes.subspan(2));
        if (!revoke) return std::nullopt;
        return ProtocolOperationV2{*revoke};
    }
    case ProtocolOperationKindV2::RECOVERY_ROTATE: {
        if (bytes.size() != 2 + RECOVERY_ROTATE_V2_SIZE) return std::nullopt;
        const auto rotate = DeserializeRecoveryRotate(bytes.subspan(2));
        if (!rotate) return std::nullopt;
        return ProtocolOperationV2{*rotate};
    }
    case ProtocolOperationKindV2::SYSTEM_LOCK: {
        if (bytes.size() != 2 + AUTHORIZED_SYSTEM_LOCK_V2_SIZE) return std::nullopt;
        const auto lock = DeserializeSystemLock(bytes.subspan(2));
        if (!lock) return std::nullopt;
        return ProtocolOperationV2{*lock};
    }
    case ProtocolOperationKindV2::NAME_COMMIT: {
        if (bytes.size() != 2 + AUTHORIZED_NAME_COMMIT_V2_SIZE) return std::nullopt;
        const auto commit = DeserializeNameCommit(bytes.subspan(2));
        if (!commit) return std::nullopt;
        return ProtocolOperationV2{*commit};
    }
    case ProtocolOperationKindV2::NAME_REVEAL: {
        if (bytes.size() != 2 + AUTHORIZED_NAME_REVEAL_V2_SIZE) return std::nullopt;
        const auto reveal = DeserializeNameReveal(bytes.subspan(2));
        if (!reveal) return std::nullopt;
        return ProtocolOperationV2{*reveal};
    }
    case ProtocolOperationKindV2::MAIL: {
        const auto mail = DeserializeAuthorizedMail(bytes.subspan(2));
        if (!mail) return std::nullopt;
        return ProtocolOperationV2{*mail};
    }
    default:
        return std::nullopt;
    }
}

std::optional<uint256> ComputeOperationIdV2(const ProtocolOperationV2& operation)
{
    constexpr std::string_view domain{"CYBOU/OP-ID/V2"};
    const auto bytes = SerializeProtocolOperationV2(operation);
    if (!bytes) return std::nullopt;
    using DigestCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    DigestCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    uint256 id;
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), bytes->data(), bytes->size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), id.begin(), &size) != 1 || size != id.size()) return std::nullopt;
    return id;
}
} // namespace cybou
