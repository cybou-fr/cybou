// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_MAIL_TX_H
#define CYBOU_MAIL_TX_H

#include <crypto/sha256.h>
#include <cybou/account_id.h>
#include <cybou/identity_registry_v2.h>
#include <cybou/protocol_params.h>
#include <uint256.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t MAIL_TX_VERSION{2};
inline constexpr size_t MAIL_PAYLOAD_HEADER_SIZE{1 + 32 + 32 + 32 + 4}; // 101 bytes
inline constexpr size_t AUTHORIZED_MAIL_HEADER_SIZE{2597 + MAIL_PAYLOAD_HEADER_SIZE}; // 2698 bytes

struct MailPayload {
    uint8_t version{MAIL_TX_VERSION};
    AccountId recipient;
    uint256 discovery_tag;
    uint256 content_commitment;
    std::vector<unsigned char> ciphertext;

    friend bool operator==(const MailPayload&, const MailPayload&) = default;
};

inline std::optional<std::vector<unsigned char>> SerializeMailPayload(const MailPayload& payload)
{
    if (payload.version != MAIL_TX_VERSION || payload.recipient.IsNull() ||
        payload.discovery_tag.IsNull() || payload.content_commitment.IsNull() ||
        payload.ciphertext.empty() || payload.ciphertext.size() > DEFAULT_MAX_MAIL_CIPHERTEXT_SIZE) {
        return std::nullopt;
    }
    std::vector<unsigned char> out;
    out.reserve(MAIL_PAYLOAD_HEADER_SIZE + payload.ciphertext.size());
    out.push_back(payload.version);
    out.insert(out.end(), payload.recipient.Value().begin(), payload.recipient.Value().end());
    out.insert(out.end(), payload.discovery_tag.begin(), payload.discovery_tag.end());
    out.insert(out.end(), payload.content_commitment.begin(), payload.content_commitment.end());
    const uint32_t len = static_cast<uint32_t>(payload.ciphertext.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(len >> (8 * i)));
    out.insert(out.end(), payload.ciphertext.begin(), payload.ciphertext.end());
    return out;
}

inline std::optional<MailPayload> DeserializeMailPayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() < MAIL_PAYLOAD_HEADER_SIZE || bytes[0] != MAIL_TX_VERSION) return std::nullopt;
    const auto recipient = AccountId::FromBytes(bytes.subspan(1, 32));
    if (!recipient || recipient->IsNull()) return std::nullopt;
    uint256 discovery_tag;
    std::copy_n(bytes.begin() + 33, 32, discovery_tag.begin());
    if (discovery_tag.IsNull()) return std::nullopt;
    uint256 content_commitment;
    std::copy_n(bytes.begin() + 65, 32, content_commitment.begin());
    if (content_commitment.IsNull()) return std::nullopt;

    uint32_t len{0};
    for (int i = 0; i < 4; ++i) len |= uint32_t{bytes[97 + i]} << (8 * i);
    if (len == 0 || len > DEFAULT_MAX_MAIL_CIPHERTEXT_SIZE || bytes.size() != MAIL_PAYLOAD_HEADER_SIZE + len) {
        return std::nullopt;
    }

    MailPayload payload;
    payload.version = bytes[0];
    payload.recipient = *recipient;
    payload.discovery_tag = discovery_tag;
    payload.content_commitment = content_commitment;
    payload.ciphertext.assign(bytes.begin() + MAIL_PAYLOAD_HEADER_SIZE, bytes.end());
    return payload;
}

inline std::optional<IdentityKeyIdV2> ComputeMailPayloadCommitment(const MailPayload& payload)
{
    static constexpr std::string_view DOMAIN{"CYBOU/MAIL-PAYLOAD/V2"};
    const auto bytes = SerializeMailPayload(payload);
    if (!bytes) return std::nullopt;
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes->data(), bytes->size());
    IdentityKeyIdV2 res{};
    hasher.Finalize(res.data());
    return res;
}

struct AuthorizedMail {
    DeviceAuthorizationV2 authorization;
    MailPayload mail;

    friend bool operator==(const AuthorizedMail&, const AuthorizedMail&) = default;
};

inline std::optional<std::vector<unsigned char>> SerializeAuthorizedMail(const AuthorizedMail& op)
{
    const auto body = SerializeMailPayload(op.mail);
    if (!body) return std::nullopt;
    const auto commitment = ComputeMailPayloadCommitment(op.mail);
    if (!commitment || *commitment != op.authorization.payload_commitment) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(2597 + body->size());
    // SerializeDeviceAuthorization requires DeviceOperationKindV2::MAIL
    if (op.authorization.kind != DeviceOperationKindV2::MAIL || op.authorization.account_id.IsNull() ||
        op.authorization.signature.ml_dsa.size() != 2420 ||
        std::all_of(op.authorization.device_id.begin(), op.authorization.device_id.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(op.authorization.payload_commitment.begin(), op.authorization.payload_commitment.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(op.authorization.signature.ed25519.begin(), op.authorization.signature.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(op.authorization.signature.ml_dsa.begin(), op.authorization.signature.ml_dsa.end(), [](unsigned char b) { return b == 0; })) {
        return std::nullopt;
    }
    out.insert(out.end(), op.authorization.account_id.Value().begin(), op.authorization.account_id.Value().end());
    out.insert(out.end(), op.authorization.device_id.begin(), op.authorization.device_id.end());
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(op.authorization.nonce >> (8 * i)));
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(op.authorization.activation_nonce >> (8 * i)));
    out.push_back(static_cast<unsigned char>(op.authorization.kind));
    out.insert(out.end(), op.authorization.payload_commitment.begin(), op.authorization.payload_commitment.end());
    out.insert(out.end(), op.authorization.signature.ed25519.begin(), op.authorization.signature.ed25519.end());
    out.insert(out.end(), op.authorization.signature.ml_dsa.begin(), op.authorization.signature.ml_dsa.end());
    out.insert(out.end(), body->begin(), body->end());
    return out;
}

inline std::optional<AuthorizedMail> DeserializeAuthorizedMail(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 2597 + MAIL_PAYLOAD_HEADER_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.first(32));
    if (!account) return std::nullopt;
    DeviceAuthorizationV2 auth{};
    auth.account_id = *account;
    size_t offset{32};
    std::copy_n(bytes.begin() + offset, 32, auth.device_id.begin());
    offset += 32;
    uint64_t nonce{0};
    for (int i = 0; i < 8; ++i) nonce |= uint64_t{bytes[offset + i]} << (8 * i);
    auth.nonce = nonce;
    offset += 8;
    uint64_t activation_nonce{0};
    for (int i = 0; i < 8; ++i) activation_nonce |= uint64_t{bytes[offset + i]} << (8 * i);
    auth.activation_nonce = activation_nonce;
    offset += 8;
    if (bytes[offset++] != static_cast<uint8_t>(DeviceOperationKindV2::MAIL)) return std::nullopt;
    auth.kind = DeviceOperationKindV2::MAIL;
    std::copy_n(bytes.begin() + offset, 32, auth.payload_commitment.begin());
    offset += 32;
    std::copy_n(bytes.begin() + offset, 64, auth.signature.ed25519.begin());
    offset += 64;
    auth.signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 2420);
    offset += 2420;

    const auto mail = DeserializeMailPayload(bytes.subspan(offset));
    if (!mail) return std::nullopt;
    const auto commitment = ComputeMailPayloadCommitment(*mail);
    if (!commitment || *commitment != auth.payload_commitment) return std::nullopt;
    return AuthorizedMail{.authorization = std::move(auth), .mail = std::move(*mail)};
}

enum class MailError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    SENDER_NOT_FOUND,
    RECIPIENT_NOT_FOUND,
    INSUFFICIENT_SYSTEM_BALANCE,
    MAIL_QUOTA_EXCEEDED,
    FEE_POOL_OVERFLOW,
    INCONSISTENT_STATE,
};

// Aliases
using MailTx = AuthorizedMail;
using MailOp = AuthorizedMail;

} // namespace cybou

#endif // CYBOU_MAIL_TX_H
