// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/protocol_operation.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <string_view>

namespace cybou {
namespace {
constexpr size_t RECOVERY_PUBLIC_SIZE{32 + 1952};
constexpr size_t AUTHORIZATION_PUBLIC_SIZE{32 + 1312};
constexpr size_t ROOT_SIGNATURE_SIZE{64 + 3309};
constexpr size_t AUTHORIZATION_SIGNATURE_SIZE{64 + 2420};

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

bool SerializeIdentityOperationAuthorization(const IdentityOperationAuthorization& auth,
    IdentityOperationKind expected_kind, std::vector<unsigned char>& out)
{
    if (auth.kind != expected_kind || auth.account_id.IsNull() || auth.signature.ml_dsa.size() != 2420 ||
        IsAllZero(auth.payload_commitment) || IsAllZero(auth.signature.ed25519) || IsAllZero(auth.signature.ml_dsa)) return false;
    out.insert(out.end(), auth.account_id.Value().begin(), auth.account_id.Value().end());
    Write64(out, auth.nonce);
    Write64(out, auth.key_epoch);
    out.push_back(static_cast<unsigned char>(auth.kind));
    out.insert(out.end(), auth.payload_commitment.begin(), auth.payload_commitment.end());
    out.insert(out.end(), auth.signature.ed25519.begin(), auth.signature.ed25519.end());
    out.insert(out.end(), auth.signature.ml_dsa.begin(), auth.signature.ml_dsa.end());
    return true;
}

std::optional<IdentityOperationAuthorization> DeserializeIdentityOperationAuthorization(
    std::span<const unsigned char> bytes, IdentityOperationKind expected_kind)
{
    if (bytes.size() != IDENTITY_OPERATION_AUTH_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.first(32));
    if (!account) return std::nullopt;
    IdentityOperationAuthorization auth{};
    auth.account_id = *account;
    size_t offset{32};
    auth.nonce = Read64(bytes.subspan(offset, 8)); offset += 8;
    auth.key_epoch = Read64(bytes.subspan(offset, 8)); offset += 8;
    if (bytes[offset++] != static_cast<uint8_t>(expected_kind)) return std::nullopt;
    auth.kind = expected_kind;
    std::copy_n(bytes.begin() + offset, 32, auth.payload_commitment.begin()); offset += 32;
    std::copy_n(bytes.begin() + offset, 64, auth.signature.ed25519.begin()); offset += 64;
    auth.signature.ml_dsa.assign(bytes.begin() + offset, bytes.end());
    if (IsAllZero(auth.payload_commitment) || IsAllZero(auth.signature.ed25519) || IsAllZero(auth.signature.ml_dsa)) return std::nullopt;
    return auth;
}

template <typename Payload, typename Commitment>
std::optional<std::vector<unsigned char>> SerializeAuthorizedPayload(
    const IdentityOperationAuthorization& auth, IdentityOperationKind kind,
    const Payload& payload, Commitment commitment_fn,
    auto serialize_fn)
{
    const auto payload_bytes = serialize_fn(payload);
    const auto commitment = commitment_fn(payload);
    if (!payload_bytes || !commitment || *commitment != auth.payload_commitment) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(IDENTITY_OPERATION_AUTH_SIZE + payload_bytes->size());
    if (!SerializeIdentityOperationAuthorization(auth, kind, out)) return std::nullopt;
    out.insert(out.end(), payload_bytes->begin(), payload_bytes->end());
    return out;
}

std::optional<std::vector<unsigned char>> SerializePayment(const AuthorizedPayment& op)
{
    return SerializeAuthorizedPayload(op.authorization, IdentityOperationKind::PAYMENT, op.payment,
        ComputePaymentPayloadCommitment, SerializePaymentPayload);
}

std::optional<AuthorizedPayment> DeserializePayment(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_PAYMENT_SIZE) return std::nullopt;
    const auto auth = DeserializeIdentityOperationAuthorization(bytes.first(IDENTITY_OPERATION_AUTH_SIZE), IdentityOperationKind::PAYMENT);
    const auto payload = DeserializePaymentPayload(bytes.subspan(IDENTITY_OPERATION_AUTH_SIZE));
    if (!auth || !payload || ComputePaymentPayloadCommitment(*payload) != auth->payload_commitment) return std::nullopt;
    return AuthorizedPayment{.authorization = *auth, .payment = *payload};
}

std::optional<std::vector<unsigned char>> SerializeSystemLock(const AuthorizedSystemLock& op)
{
    return SerializeAuthorizedPayload(op.authorization, IdentityOperationKind::SYSTEM_LOCK, op.lock,
        ComputeSystemLockPayloadCommitment, SerializeSystemLockPayload);
}

std::optional<AuthorizedSystemLock> DeserializeSystemLock(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_SYSTEM_LOCK_SIZE) return std::nullopt;
    const auto auth = DeserializeIdentityOperationAuthorization(bytes.first(IDENTITY_OPERATION_AUTH_SIZE), IdentityOperationKind::SYSTEM_LOCK);
    const auto payload = DeserializeSystemLockPayload(bytes.subspan(IDENTITY_OPERATION_AUTH_SIZE));
    if (!auth || !payload || ComputeSystemLockPayloadCommitment(*payload) != auth->payload_commitment) return std::nullopt;
    return AuthorizedSystemLock{.authorization = *auth, .lock = *payload};
}

std::optional<std::vector<unsigned char>> SerializeNameCommit(const AuthorizedNameCommit& op)
{
    return SerializeAuthorizedPayload(op.authorization, IdentityOperationKind::NAME_COMMIT, op.commit,
        ComputeNameCommitPayloadCommitment, SerializeNameCommitPayload);
}

std::optional<AuthorizedNameCommit> DeserializeNameCommit(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_NAME_COMMIT_SIZE) return std::nullopt;
    const auto auth = DeserializeIdentityOperationAuthorization(bytes.first(IDENTITY_OPERATION_AUTH_SIZE), IdentityOperationKind::NAME_COMMIT);
    const auto payload = DeserializeNameCommitPayload(bytes.subspan(IDENTITY_OPERATION_AUTH_SIZE));
    if (!auth || !payload || ComputeNameCommitPayloadCommitment(*payload) != auth->payload_commitment) return std::nullopt;
    return AuthorizedNameCommit{.authorization = *auth, .commit = *payload};
}

std::optional<std::vector<unsigned char>> SerializeNameReveal(const AuthorizedNameReveal& op)
{
    return SerializeAuthorizedPayload(op.authorization, IdentityOperationKind::NAME_REVEAL, op.reveal,
        ComputeNameRevealPayloadCommitment, SerializeNameRevealPayload);
}

std::optional<AuthorizedNameReveal> DeserializeNameReveal(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_NAME_REVEAL_SIZE) return std::nullopt;
    const auto auth = DeserializeIdentityOperationAuthorization(bytes.first(IDENTITY_OPERATION_AUTH_SIZE), IdentityOperationKind::NAME_REVEAL);
    const auto payload = DeserializeNameRevealPayload(bytes.subspan(IDENTITY_OPERATION_AUTH_SIZE));
    if (!auth || !payload || ComputeNameRevealPayloadCommitment(*payload) != auth->payload_commitment) return std::nullopt;
    return AuthorizedNameReveal{.authorization = *auth, .reveal = *payload};
}

std::optional<std::vector<unsigned char>> SerializeRootPublicationOperation(const AuthorizedRootPublication& op)
{
    return SerializeAuthorizedPayload(op.authorization, IdentityOperationKind::ROOT_PUBLICATION, op.publication,
        ComputeRootPublicationPayloadCommitment, SerializeRootPublication);
}

std::optional<AuthorizedRootPublication> DeserializeRootPublicationOperation(std::span<const unsigned char> bytes)
{
    if (bytes.size() < IDENTITY_OPERATION_AUTH_SIZE || bytes.size() > ROOT_PUBLICATION_MAX_OPERATION_BYTES) {
        return std::nullopt;
    }
    const auto auth = DeserializeIdentityOperationAuthorization(bytes.first(IDENTITY_OPERATION_AUTH_SIZE),
        IdentityOperationKind::ROOT_PUBLICATION);
    const auto publication = DeserializeRootPublication(bytes.subspan(IDENTITY_OPERATION_AUTH_SIZE));
    if (!auth || !publication || ComputeRootPublicationPayloadCommitment(*publication) != auth->payload_commitment) {
        return std::nullopt;
    }
    return AuthorizedRootPublication{.authorization = *auth, .publication = *publication};
}

void WritePublic(std::vector<unsigned char>& out, const IdentityHybridPublicKey& key)
{
    out.insert(out.end(), key.ed25519.begin(), key.ed25519.end());
    out.insert(out.end(), key.ml_dsa.begin(), key.ml_dsa.end());
}

void WriteSignature(std::vector<unsigned char>& out, const IdentityHybridSignature& signature)
{
    out.insert(out.end(), signature.ed25519.begin(), signature.ed25519.end());
    out.insert(out.end(), signature.ml_dsa.begin(), signature.ml_dsa.end());
}

std::optional<std::vector<unsigned char>> SerializeIdentityRotate(const IdentityRotate& op)
{
    if (op.account_id.IsNull() || op.new_recovery_key.purpose != IdentityKeyPurpose::RECOVERY_ROOT ||
        op.new_authorization_key.purpose != IdentityKeyPurpose::AUTHORIZATION ||
        op.new_recovery_key.ml_dsa.size() != 1952 || op.new_authorization_key.ml_dsa.size() != 1312 ||
        op.old_recovery_signature.ml_dsa.size() != 3309 || op.new_recovery_pop.ml_dsa.size() != 3309 ||
        op.new_authorization_pop.ml_dsa.size() != 2420 || !DecodeIdentityKemPackage(op.new_kem_package) ||
        IsAllZero(op.new_recovery_key.ed25519) || IsAllZero(op.new_recovery_key.ml_dsa) ||
        IsAllZero(op.new_authorization_key.ed25519) || IsAllZero(op.new_authorization_key.ml_dsa) ||
        IsAllZero(op.old_recovery_signature.ed25519) || IsAllZero(op.old_recovery_signature.ml_dsa) ||
        IsAllZero(op.new_recovery_pop.ed25519) || IsAllZero(op.new_recovery_pop.ml_dsa) ||
        IsAllZero(op.new_authorization_pop.ed25519) || IsAllZero(op.new_authorization_pop.ml_dsa)) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(IDENTITY_ROTATE_SIZE);
    out.insert(out.end(), op.account_id.Value().begin(), op.account_id.Value().end());
    WritePublic(out, op.new_recovery_key);
    WritePublic(out, op.new_authorization_key);
    out.insert(out.end(), op.new_kem_package.begin(), op.new_kem_package.end());
    Write64(out, op.nonce);
    Write64(out, op.key_epoch);
    WriteSignature(out, op.old_recovery_signature);
    WriteSignature(out, op.new_recovery_pop);
    WriteSignature(out, op.new_authorization_pop);
    if (out.size() != IDENTITY_ROTATE_SIZE) return std::nullopt;
    return out;
}

std::optional<IdentityRotate> DeserializeIdentityRotate(std::span<const unsigned char> bytes)
{
    if (bytes.size() != IDENTITY_ROTATE_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.first(32));
    if (!account) return std::nullopt;
    IdentityRotate op{};
    op.account_id = *account;
    op.new_recovery_key.purpose = IdentityKeyPurpose::RECOVERY_ROOT;
    op.new_authorization_key.purpose = IdentityKeyPurpose::AUTHORIZATION;
    size_t offset{32};
    std::copy_n(bytes.begin() + offset, 32, op.new_recovery_key.ed25519.begin()); offset += 32;
    op.new_recovery_key.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 1952); offset += 1952;
    std::copy_n(bytes.begin() + offset, 32, op.new_authorization_key.ed25519.begin()); offset += 32;
    op.new_authorization_key.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 1312); offset += 1312;
    std::copy_n(bytes.begin() + offset, op.new_kem_package.size(), op.new_kem_package.begin()); offset += op.new_kem_package.size();
    if (!DecodeIdentityKemPackage(op.new_kem_package)) return std::nullopt;
    op.nonce = Read64(bytes.subspan(offset, 8)); offset += 8;
    op.key_epoch = Read64(bytes.subspan(offset, 8)); offset += 8;
    auto read_signature = [&](IdentityHybridSignature& sig, size_t pq_size) {
        std::copy_n(bytes.begin() + offset, 64, sig.ed25519.begin()); offset += 64;
        sig.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + pq_size); offset += pq_size;
    };
    read_signature(op.old_recovery_signature, 3309);
    read_signature(op.new_recovery_pop, 3309);
    read_signature(op.new_authorization_pop, 2420);
    if (offset != bytes.size()) return std::nullopt;
    return SerializeIdentityRotate(op) ? std::optional<IdentityRotate>{std::move(op)} : std::nullopt;
}
} // namespace

std::optional<std::vector<unsigned char>> SerializeProtocolOperation(const ProtocolOperation& operation)
{
    std::vector<unsigned char> out{PROTOCOL_OPERATION_VERSION};
    if (const auto* create = std::get_if<AccountCreateOp>(&operation)) {
        const auto body = SerializeAccountCreateOp(*create);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKind::ACCOUNT_CREATE));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* payment = std::get_if<AuthorizedPayment>(&operation)) {
        const auto body = SerializePayment(*payment);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKind::PAYMENT));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* rotate = std::get_if<IdentityRotate>(&operation)) {
        const auto body = SerializeIdentityRotate(*rotate);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKind::IDENTITY_ROTATE));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* lock = std::get_if<AuthorizedSystemLock>(&operation)) {
        const auto body = SerializeSystemLock(*lock);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKind::SYSTEM_LOCK));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* commit = std::get_if<AuthorizedNameCommit>(&operation)) {
        const auto body = SerializeNameCommit(*commit);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKind::NAME_COMMIT));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* reveal = std::get_if<AuthorizedNameReveal>(&operation)) {
        const auto body = SerializeNameReveal(*reveal);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKind::NAME_REVEAL));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* mail = std::get_if<AuthorizedMail>(&operation)) {
        const auto body = SerializeAuthorizedMail(*mail);
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKind::MAIL));
        out.insert(out.end(), body->begin(), body->end());
    } else if (const auto* publication = std::get_if<AuthorizedRootPublication>(&operation)) {
        const auto body = SerializeRootPublicationOperation(*publication);
        if (!body || body->size() + 2 > ROOT_PUBLICATION_MAX_OPERATION_BYTES) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKind::ROOT_PUBLICATION));
        out.insert(out.end(), body->begin(), body->end());
    } else {
        return std::nullopt;
    }
    return out;
}

std::optional<ProtocolOperation> DeserializeProtocolOperation(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 2 || bytes[0] != PROTOCOL_OPERATION_VERSION) return std::nullopt;
    const auto kind = static_cast<ProtocolOperationKind>(bytes[1]);
    switch (kind) {
    case ProtocolOperationKind::ACCOUNT_CREATE: {
        if (bytes.size() != 2 + ACCOUNT_CREATE_SIZE) return std::nullopt;
        const auto op = DeserializeAccountCreateOp(bytes.subspan(2));
        return op ? std::optional<ProtocolOperation>{ProtocolOperation{*op}} : std::nullopt;
    }
    case ProtocolOperationKind::PAYMENT: {
        if (bytes.size() != 2 + AUTHORIZED_PAYMENT_SIZE) return std::nullopt;
        const auto op = DeserializePayment(bytes.subspan(2));
        return op ? std::optional<ProtocolOperation>{ProtocolOperation{*op}} : std::nullopt;
    }
    case ProtocolOperationKind::IDENTITY_ROTATE: {
        if (bytes.size() != 2 + IDENTITY_ROTATE_SIZE) return std::nullopt;
        const auto op = DeserializeIdentityRotate(bytes.subspan(2));
        return op ? std::optional<ProtocolOperation>{ProtocolOperation{*op}} : std::nullopt;
    }
    case ProtocolOperationKind::SYSTEM_LOCK: {
        if (bytes.size() != 2 + AUTHORIZED_SYSTEM_LOCK_SIZE) return std::nullopt;
        const auto op = DeserializeSystemLock(bytes.subspan(2));
        return op ? std::optional<ProtocolOperation>{ProtocolOperation{*op}} : std::nullopt;
    }
    case ProtocolOperationKind::NAME_COMMIT: {
        if (bytes.size() < 2 + AUTHORIZED_NAME_COMMIT_SIZE) return std::nullopt;
        const auto op = DeserializeNameCommit(bytes.subspan(2));
        return op ? std::optional<ProtocolOperation>{ProtocolOperation{*op}} : std::nullopt;
    }
    case ProtocolOperationKind::NAME_REVEAL: {
        if (bytes.size() < 2 + AUTHORIZED_NAME_REVEAL_SIZE) return std::nullopt;
        const auto op = DeserializeNameReveal(bytes.subspan(2));
        return op ? std::optional<ProtocolOperation>{ProtocolOperation{*op}} : std::nullopt;
    }
    case ProtocolOperationKind::MAIL: {
        const auto op = DeserializeAuthorizedMail(bytes.subspan(2));
        return op ? std::optional<ProtocolOperation>{ProtocolOperation{*op}} : std::nullopt;
    }
    case ProtocolOperationKind::ROOT_PUBLICATION: {
        if (bytes.size() > ROOT_PUBLICATION_MAX_OPERATION_BYTES) return std::nullopt;
        const auto op = DeserializeRootPublicationOperation(bytes.subspan(2));
        return op ? std::optional<ProtocolOperation>{ProtocolOperation{*op}} : std::nullopt;
    }
    default: return std::nullopt;
    }
}

std::optional<uint256> ComputeOperationId(const ProtocolOperation& operation)
{
    constexpr std::string_view domain{"CYBOU/OP-ID/V4"};
    const auto bytes = SerializeProtocolOperation(operation);
    if (!bytes) return std::nullopt;
    uint256 id;
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*bytes}}, id.begin())) return std::nullopt;
    return id;
}

} // namespace cybou
