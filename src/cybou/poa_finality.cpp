// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/poa_finality.h>

#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <string_view>
#include <utility>

namespace cybou {
namespace {

void AppendUint64LE(std::vector<unsigned char>& out, const uint64_t value)
{
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint64_t ReadUint64LE(const std::span<const unsigned char> bytes, const size_t offset)
{
    uint64_t value{0};
    for (int i = 0; i < 8; ++i) value |= uint64_t{bytes[offset + i]} << (8 * i);
    return value;
}

bool Nonzero(const std::span<const unsigned char> bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](const unsigned char value) { return value != 0; });
}

bool ValidFields(const PoaFinalityCertificate& certificate)
{
    return certificate.version == POA_FINALITY_CERTIFICATE_VERSION &&
        !certificate.network_id.IsNull() && !certificate.block_id.IsNull() &&
        certificate.height != 0 && !certificate.parent_block_id.IsNull() &&
        certificate.signature.ml_dsa.size() == 3309 &&
        Nonzero(certificate.signature.ed25519) && Nonzero(certificate.signature.ml_dsa);
}

} // namespace

uint256 ComputePoaFinalityDigest(const uint256& network_id, const uint256& block_id,
    const uint64_t height, const uint256& parent_block_id)
{
    static constexpr std::string_view DOMAIN{"CYBOU/POA_FINALITY/V1"};
    unsigned char height_bytes[8];
    for (int i = 0; i < 8; ++i) height_bytes[i] = static_cast<unsigned char>(height >> (8 * i));

    uint256 digest;
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());
    hasher.Write(block_id.begin(), block_id.size());
    hasher.Write(height_bytes, sizeof(height_bytes));
    hasher.Write(parent_block_id.begin(), parent_block_id.size());
    hasher.Finalize(digest.begin());
    return digest;
}

std::optional<PoaFinalityCertificate> SignPoaFinalityCertificate(
    const std::span<const unsigned char, 32> operator_recovery_entropy,
    const uint256& network_id, const uint256& block_id, const uint64_t height,
    const uint256& parent_block_id)
{
    if (!Nonzero(operator_recovery_entropy) || network_id.IsNull() || block_id.IsNull() ||
        height == 0 || parent_block_id.IsNull()) return std::nullopt;
    const auto digest = ComputePoaFinalityDigest(network_id, block_id, height, parent_block_id);
    const auto signature = SignIdentityMessage(operator_recovery_entropy,
        IdentityKeyPurpose::POA_FINALIZER, digest);
    if (!signature) return std::nullopt;
    PoaFinalityCertificate certificate{
        .version = POA_FINALITY_CERTIFICATE_VERSION,
        .network_id = network_id,
        .block_id = block_id,
        .height = height,
        .parent_block_id = parent_block_id,
        .signature = *signature,
    };
    return ValidFields(certificate) ? std::optional<PoaFinalityCertificate>{std::move(certificate)} : std::nullopt;
}

bool VerifyPoaFinalityCertificate(const PoaFinalityCertificate& certificate,
    const IdentityHybridPublicKey& genesis_finalizer_key,
    const uint256& expected_network_id, const uint256& expected_block_id,
    const uint64_t expected_height, const uint256& expected_parent_block_id)
{
    if (!ValidFields(certificate) ||
        genesis_finalizer_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        genesis_finalizer_key.ml_dsa.size() != 1952 ||
        !Nonzero(genesis_finalizer_key.ed25519) || !Nonzero(genesis_finalizer_key.ml_dsa) ||
        certificate.network_id != expected_network_id || certificate.block_id != expected_block_id ||
        certificate.height != expected_height || certificate.parent_block_id != expected_parent_block_id) {
        return false;
    }
    const auto digest = ComputePoaFinalityDigest(certificate.network_id, certificate.block_id,
        certificate.height, certificate.parent_block_id);
    return VerifyIdentityMessage(genesis_finalizer_key, certificate.signature, digest);
}

std::optional<std::vector<unsigned char>> SerializePoaFinalityCertificate(
    const PoaFinalityCertificate& certificate)
{
    if (!ValidFields(certificate)) return std::nullopt;
    std::vector<unsigned char> bytes;
    bytes.reserve(POA_FINALITY_CERTIFICATE_SIZE);
    bytes.push_back(certificate.version);
    bytes.insert(bytes.end(), certificate.network_id.begin(), certificate.network_id.end());
    bytes.insert(bytes.end(), certificate.block_id.begin(), certificate.block_id.end());
    AppendUint64LE(bytes, certificate.height);
    bytes.insert(bytes.end(), certificate.parent_block_id.begin(), certificate.parent_block_id.end());
    bytes.insert(bytes.end(), certificate.signature.ed25519.begin(), certificate.signature.ed25519.end());
    bytes.insert(bytes.end(), certificate.signature.ml_dsa.begin(), certificate.signature.ml_dsa.end());
    return bytes;
}

std::optional<PoaFinalityCertificate> DeserializePoaFinalityCertificate(
    const std::span<const unsigned char> bytes)
{
    if (bytes.size() != POA_FINALITY_CERTIFICATE_SIZE || bytes[0] != POA_FINALITY_CERTIFICATE_VERSION) {
        return std::nullopt;
    }
    PoaFinalityCertificate certificate;
    certificate.version = bytes[0];
    size_t offset{1};
    std::copy_n(bytes.begin() + offset, 32, certificate.network_id.begin());
    offset += 32;
    std::copy_n(bytes.begin() + offset, 32, certificate.block_id.begin());
    offset += 32;
    certificate.height = ReadUint64LE(bytes, offset);
    offset += 8;
    std::copy_n(bytes.begin() + offset, 32, certificate.parent_block_id.begin());
    offset += 32;
    std::copy_n(bytes.begin() + offset, certificate.signature.ed25519.size(), certificate.signature.ed25519.begin());
    offset += certificate.signature.ed25519.size();
    certificate.signature.ml_dsa.assign(bytes.begin() + offset, bytes.end());
    if (!ValidFields(certificate)) return std::nullopt;
    return certificate;
}

} // namespace cybou
