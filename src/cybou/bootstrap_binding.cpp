// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_binding.h>

#include <cybou/crypto/sha256.h>
#include <cybou/signing.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <utility>

namespace cybou {
namespace {

constexpr std::array<unsigned char, 5> MAGIC{'C', 'Y', 'B', 'B', '1'};
constexpr size_t MLDSA65_SIGNATURE_SIZE{3309};
constexpr std::string_view SIGNING_DOMAIN{"CYBOU/BOOTSTRAP/NETWORK-BINDING/v1"};

void AppendU16(std::vector<unsigned char>& out, const uint16_t value)
{
    out.push_back(static_cast<unsigned char>(value));
    out.push_back(static_cast<unsigned char>(value >> 8));
}

void AppendU32(std::vector<unsigned char>& out, const uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

void AppendU64(std::vector<unsigned char>& out, const uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

bool ReadU16(std::span<const unsigned char> bytes, size_t& offset, uint16_t& value)
{
    if (offset > bytes.size() || bytes.size() - offset < 2) return false;
    value = static_cast<uint16_t>(bytes[offset]) | (static_cast<uint16_t>(bytes[offset + 1]) << 8);
    offset += 2;
    return true;
}

bool ReadU32(std::span<const unsigned char> bytes, size_t& offset, uint32_t& value)
{
    if (offset > bytes.size() || bytes.size() - offset < 4) return false;
    value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t{bytes[offset + i]} << (8 * i);
    offset += 4;
    return true;
}

bool ReadU64(std::span<const unsigned char> bytes, size_t& offset, uint64_t& value)
{
    if (offset > bytes.size() || bytes.size() - offset < 8) return false;
    value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= uint64_t{bytes[offset + i]} << (8 * i);
    offset += 8;
    return true;
}

bool ValidDisplayName(const std::string& name)
{
    return !name.empty() && name.size() <= MAX_BOOTSTRAP_DISPLAY_NAME_BYTES &&
        std::none_of(name.begin(), name.end(), [](unsigned char byte) { return byte == 0 || byte < 0x20; });
}

std::optional<std::array<unsigned char, 32>> NetworkFileHash(std::span<const unsigned char> bytes)
{
    if (bytes.empty() || bytes.size() > MAX_BOOTSTRAP_NETWORK_FILE_BYTES) return std::nullopt;
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({bytes}, digest.data())) return std::nullopt;
    return digest;
}

std::vector<unsigned char> SigningMessage(const BootstrapNetworkBinding& binding,
    const IdentityHybridPublicKey& finalizer_key)
{
    std::vector<unsigned char> message;
    message.insert(message.end(), SIGNING_DOMAIN.begin(), SIGNING_DOMAIN.end());
    AppendU64(message, binding.generation);
    AppendU16(message, static_cast<uint16_t>(binding.display_name.size()));
    message.insert(message.end(), binding.display_name.begin(), binding.display_name.end());
    message.insert(message.end(), binding.network_id.begin(), binding.network_id.end());
    message.insert(message.end(), binding.network_file_sha256.begin(), binding.network_file_sha256.end());
    message.push_back(static_cast<unsigned char>(finalizer_key.purpose));
    message.insert(message.end(), finalizer_key.ed25519.begin(), finalizer_key.ed25519.end());
    AppendU16(message, static_cast<uint16_t>(finalizer_key.ml_dsa.size()));
    message.insert(message.end(), finalizer_key.ml_dsa.begin(), finalizer_key.ml_dsa.end());
    return message;
}

} // namespace

std::optional<BootstrapNetworkBinding> CreateBootstrapNetworkBinding(
    const uint64_t generation, std::string display_name,
    const std::span<const unsigned char> exact_network_file, const RecoveryEntropy& poa_recovery_entropy)
{
    if (generation == 0 || !ValidDisplayName(display_name)) return std::nullopt;
    const auto network_hash = NetworkFileHash(exact_network_file);
    const auto network = DeserializeCybouNetworkFile(exact_network_file);
    const auto finalizer_key = DeriveIdentityPublicKey(poa_recovery_entropy, IdentityKeyPurpose::POA_FINALIZER);
    if (!network_hash || !network || !finalizer_key ||
        network->definition.poa_finalizer_public_key != *finalizer_key) return std::nullopt;

    BootstrapNetworkBinding binding{
        .generation = generation,
        .display_name = std::move(display_name),
        .network_id = NetworkId(network->definition),
        .network_file_sha256 = *network_hash,
        .network_file = {exact_network_file.begin(), exact_network_file.end()},
    };
    const auto message = SigningMessage(binding, network->definition.poa_finalizer_public_key);
    const auto signature = SignIdentityMessage(poa_recovery_entropy, IdentityKeyPurpose::POA_FINALIZER, message);
    if (!signature) return std::nullopt;
    binding.authority_signature = *signature;
    return binding;
}

bool VerifyBootstrapNetworkBinding(const BootstrapNetworkBinding& binding)
{
    if (binding.generation == 0 || !ValidDisplayName(binding.display_name)) return false;
    const auto network_hash = NetworkFileHash(binding.network_file);
    const auto network = DeserializeCybouNetworkFile(binding.network_file);
    if (!network_hash || !network || *network_hash != binding.network_file_sha256 ||
        NetworkId(network->definition) != binding.network_id ||
        binding.authority_signature.ml_dsa.size() != MLDSA65_SIGNATURE_SIZE) return false;
    const auto message = SigningMessage(binding, network->definition.poa_finalizer_public_key);
    return VerifyIdentityMessage(network->definition.poa_finalizer_public_key,
        binding.authority_signature, message);
}

std::optional<std::vector<unsigned char>> EncodeBootstrapNetworkBinding(
    const BootstrapNetworkBinding& binding)
{
    if (!VerifyBootstrapNetworkBinding(binding) || binding.network_file.size() > std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }
    std::vector<unsigned char> bytes(MAGIC.begin(), MAGIC.end());
    AppendU64(bytes, binding.generation);
    AppendU16(bytes, static_cast<uint16_t>(binding.display_name.size()));
    bytes.insert(bytes.end(), binding.display_name.begin(), binding.display_name.end());
    bytes.insert(bytes.end(), binding.network_id.begin(), binding.network_id.end());
    bytes.insert(bytes.end(), binding.network_file_sha256.begin(), binding.network_file_sha256.end());
    AppendU32(bytes, static_cast<uint32_t>(binding.network_file.size()));
    bytes.insert(bytes.end(), binding.network_file.begin(), binding.network_file.end());
    bytes.insert(bytes.end(), binding.authority_signature.ed25519.begin(), binding.authority_signature.ed25519.end());
    AppendU16(bytes, static_cast<uint16_t>(binding.authority_signature.ml_dsa.size()));
    bytes.insert(bytes.end(), binding.authority_signature.ml_dsa.begin(), binding.authority_signature.ml_dsa.end());
    return bytes;
}

std::optional<BootstrapNetworkBinding> DecodeBootstrapNetworkBinding(const std::span<const unsigned char> bytes)
{
    if (bytes.size() < MAGIC.size() + 8 + 2 + 64 + 4 + 64 + 2 + MLDSA65_SIGNATURE_SIZE ||
        !std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin())) return std::nullopt;
    size_t offset{MAGIC.size()};
    BootstrapNetworkBinding binding;
    uint16_t name_size{0};
    uint32_t network_file_size{0};
    if (!ReadU64(bytes, offset, binding.generation) || !ReadU16(bytes, offset, name_size) ||
        name_size == 0 || name_size > MAX_BOOTSTRAP_DISPLAY_NAME_BYTES || name_size > bytes.size() - offset) {
        return std::nullopt;
    }
    binding.display_name.assign(reinterpret_cast<const char*>(bytes.data() + offset), name_size);
    offset += name_size;
    if (bytes.size() - offset < binding.network_id.size() + binding.network_file_sha256.size()) return std::nullopt;
    std::copy_n(bytes.begin() + offset, binding.network_id.size(), binding.network_id.begin());
    offset += binding.network_id.size();
    std::copy_n(bytes.begin() + offset, binding.network_file_sha256.size(), binding.network_file_sha256.begin());
    offset += binding.network_file_sha256.size();
    if (!ReadU32(bytes, offset, network_file_size) || network_file_size == 0 ||
        network_file_size > MAX_BOOTSTRAP_NETWORK_FILE_BYTES || network_file_size > bytes.size() - offset) {
        return std::nullopt;
    }
    binding.network_file.assign(bytes.begin() + offset, bytes.begin() + offset + network_file_size);
    offset += network_file_size;
    if (bytes.size() - offset < binding.authority_signature.ed25519.size()) return std::nullopt;
    std::copy_n(bytes.begin() + offset, binding.authority_signature.ed25519.size(),
        binding.authority_signature.ed25519.begin());
    offset += binding.authority_signature.ed25519.size();
    uint16_t signature_size{0};
    if (!ReadU16(bytes, offset, signature_size) || signature_size != MLDSA65_SIGNATURE_SIZE ||
        signature_size != bytes.size() - offset) return std::nullopt;
    binding.authority_signature.ml_dsa.assign(bytes.begin() + offset, bytes.end());
    if (!VerifyBootstrapNetworkBinding(binding)) return std::nullopt;
    return binding;
}

} // namespace cybou
