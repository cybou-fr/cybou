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
constexpr size_t PAYMENT_WIRE_SIZE{2 + AUTHORIZED_PAYMENT_V2_SIZE};

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

std::optional<std::vector<unsigned char>> SerializePayment(const AuthorizedPaymentV2& operation)
{
    const auto payment = SerializePaymentPayloadV2(operation.payment);
    if (!payment || operation.authorization.kind != DeviceOperationKindV2::PAYMENT ||
        operation.authorization.account_id.IsNull() || operation.authorization.signature.ml_dsa.size() != 2356 ||
        std::all_of(operation.authorization.device_id.begin(), operation.authorization.device_id.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(operation.authorization.signature.ed25519.begin(), operation.authorization.signature.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(operation.authorization.signature.ml_dsa.begin(), operation.authorization.signature.ml_dsa.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;
    const auto commitment = ComputePaymentPayloadCommitmentV2(operation.payment);
    if (!commitment || *commitment != operation.authorization.payload_commitment) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(AUTHORIZED_PAYMENT_V2_SIZE);
    const auto& auth = operation.authorization;
    out.insert(out.end(), auth.account_id.Value().begin(), auth.account_id.Value().end());
    out.insert(out.end(), auth.device_id.begin(), auth.device_id.end());
    Write64(out, auth.nonce);
    Write64(out, auth.activation_nonce);
    out.push_back(static_cast<unsigned char>(auth.kind));
    out.insert(out.end(), auth.payload_commitment.begin(), auth.payload_commitment.end());
    out.insert(out.end(), auth.signature.ed25519.begin(), auth.signature.ed25519.end());
    out.insert(out.end(), auth.signature.ml_dsa.begin(), auth.signature.ml_dsa.end());
    out.insert(out.end(), payment->begin(), payment->end());
    if (out.size() != AUTHORIZED_PAYMENT_V2_SIZE) return std::nullopt;
    return out;
}

std::optional<AuthorizedPaymentV2> DeserializePayment(std::span<const unsigned char> bytes)
{
    if (bytes.size() != AUTHORIZED_PAYMENT_V2_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.first(32));
    if (!account) return std::nullopt;
    AuthorizedPaymentV2 result{};
    result.authorization.account_id = *account;
    size_t offset{32};
    std::copy_n(bytes.begin() + offset, 32, result.authorization.device_id.begin());
    offset += 32;
    result.authorization.nonce = Read64(bytes.subspan(offset, 8));
    offset += 8;
    result.authorization.activation_nonce = Read64(bytes.subspan(offset, 8));
    offset += 8;
    if (bytes[offset++] != static_cast<uint8_t>(DeviceOperationKindV2::PAYMENT)) return std::nullopt;
    result.authorization.kind = DeviceOperationKindV2::PAYMENT;
    std::copy_n(bytes.begin() + offset, 32, result.authorization.payload_commitment.begin());
    offset += 32;
    std::copy_n(bytes.begin() + offset, 64, result.authorization.signature.ed25519.begin());
    offset += 64;
    result.authorization.signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 2356);
    offset += 2356;
    const auto payment = DeserializePaymentPayloadV2(bytes.subspan(offset));
    if (!payment || ComputePaymentPayloadCommitmentV2(*payment) != result.authorization.payload_commitment ||
        std::all_of(result.authorization.device_id.begin(), result.authorization.device_id.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(result.authorization.signature.ed25519.begin(), result.authorization.signature.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(result.authorization.signature.ml_dsa.begin(), result.authorization.signature.ml_dsa.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;
    result.payment = *payment;
    return result;
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
    } else {
        const auto body = SerializePayment(std::get<AuthorizedPaymentV2>(operation));
        if (!body) return std::nullopt;
        out.push_back(static_cast<unsigned char>(ProtocolOperationKindV2::PAYMENT));
        out.insert(out.end(), body->begin(), body->end());
    }
    return out;
}

std::optional<ProtocolOperationV2> DeserializeProtocolOperationV2(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 2 || bytes[0] != PROTOCOL_OPERATION_VERSION_V2) return std::nullopt;
    if (bytes[1] == static_cast<uint8_t>(ProtocolOperationKindV2::ACCOUNT_CREATE)) {
        if (bytes.size() != 2 + ACCOUNT_CREATE_V2_SIZE) return std::nullopt;
        const auto create = DeserializeAccountCreateOpV2(bytes.subspan(2));
        if (!create) return std::nullopt;
        return ProtocolOperationV2{*create};
    }
    if (bytes[1] == static_cast<uint8_t>(ProtocolOperationKindV2::PAYMENT)) {
        if (bytes.size() != PAYMENT_WIRE_SIZE) return std::nullopt;
        const auto payment = DeserializePayment(bytes.subspan(2));
        if (!payment) return std::nullopt;
        return ProtocolOperationV2{*payment};
    }
    return std::nullopt;
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
