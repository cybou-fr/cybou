// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_material.h>
#include <cybou/identity_vault.h>
#include <cybou/crypto/cleanse.h>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <vector>

namespace cybou {
namespace {
constexpr std::array<unsigned char, 5> MAGIC{'C', 'V', 'I', 'D', '4'};
constexpr size_t PAYLOAD_SIZE{MAGIC.size() + 32 + 32 + 32 + XWING_SEED_SIZE};

template <size_t N>
bool Nonzero(const std::array<unsigned char, N>& value)
{
    return std::any_of(value.begin(), value.end(), [](unsigned char byte) { return byte != 0; });
}

std::optional<IdentityMaterial> Parse(std::span<const unsigned char> bytes)
{
    if (bytes.size() != PAYLOAD_SIZE || !std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin())) return std::nullopt;
    IdentityMaterial material;
    auto first = bytes.begin() + MAGIC.size();
    std::copy_n(first, 32, material.account_id.begin());
    std::copy_n(first + 32, 32, material.recovery_entropy.begin());
    std::copy_n(first + 64, 32, material.device_secret.begin());
    std::copy_n(first + 96, XWING_SEED_SIZE, material.device_xwing_seed.begin());
    if (!Nonzero(material.account_id) || !Nonzero(material.device_secret) ||
        !Nonzero(material.device_xwing_seed) || !DeriveXWingPublicKey(material.device_xwing_seed)) return std::nullopt;
    return material;
}
} // namespace

IdentityMaterial::IdentityMaterial(IdentityMaterial&& other) noexcept
    : account_id{other.account_id}, recovery_entropy{other.recovery_entropy},
      device_secret{other.device_secret}, device_xwing_seed{other.device_xwing_seed}
{
    other.Clear();
}

IdentityMaterial& IdentityMaterial::operator=(IdentityMaterial&& other) noexcept
{
    if (this != &other) {
        Clear();
        account_id = other.account_id;
        recovery_entropy = other.recovery_entropy;
        device_secret = other.device_secret;
        device_xwing_seed = other.device_xwing_seed;
        other.Clear();
    }
    return *this;
}

IdentityMaterial::~IdentityMaterial() { Clear(); }

void IdentityMaterial::Clear() noexcept
{
    crypto::CleanseMemory(account_id.data(), account_id.size());
    crypto::CleanseMemory(recovery_entropy.data(), recovery_entropy.size());
    crypto::CleanseMemory(device_secret.data(), device_secret.size());
    crypto::CleanseMemory(device_xwing_seed.data(), device_xwing_seed.size());
}

std::optional<IdentityMaterial> GenerateIdentityMaterial()
{
    IdentityMaterial material;
    auto entropy = GenerateRecoveryEntropy();
    if (!entropy) return std::nullopt;
    if (RAND_bytes(material.account_id.data(), material.account_id.size()) != 1 ||
        RAND_bytes(material.device_secret.data(), material.device_secret.size()) != 1 ||
        !Nonzero(material.account_id) || !Nonzero(material.device_secret)) {
        crypto::CleanseMemory(entropy->data(), entropy->size());
        return std::nullopt;
    }
    material.recovery_entropy = *entropy;
    crypto::CleanseMemory(entropy->data(), entropy->size());
    auto xwing_seed = GenerateXWingSeed();
    if (!xwing_seed) return std::nullopt;
    material.device_xwing_seed = *xwing_seed;
    crypto::CleanseMemory(xwing_seed->data(), xwing_seed->size());
    return material;
}

bool SaveNewIdentityMaterial(const std::filesystem::path& path,
    std::string_view password, const IdentityMaterial& material)
{
    auto payload = SerializeIdentityMaterial(material);
    if (!payload) return false;
    const bool saved = SaveNewIdentityVault(path, password, *payload);
    crypto::CleanseMemory(payload->data(), payload->size());
    return saved;
}

std::optional<std::vector<unsigned char>> SerializeIdentityMaterial(const IdentityMaterial& material)
{
    if (!Nonzero(material.account_id) || !Nonzero(material.device_secret) ||
        !Nonzero(material.device_xwing_seed) || !DeriveXWingPublicKey(material.device_xwing_seed)) return std::nullopt;
    std::vector<unsigned char> payload(PAYLOAD_SIZE);
    std::copy(MAGIC.begin(), MAGIC.end(), payload.begin());
    std::copy(material.account_id.begin(), material.account_id.end(), payload.begin() + 5);
    std::copy(material.recovery_entropy.begin(), material.recovery_entropy.end(), payload.begin() + 37);
    std::copy(material.device_secret.begin(), material.device_secret.end(), payload.begin() + 69);
    std::copy(material.device_xwing_seed.begin(), material.device_xwing_seed.end(), payload.begin() + 101);
    return payload;
}

std::optional<IdentityMaterial> LoadIdentityMaterial(
    const std::filesystem::path& path, std::string_view password)
{
    auto payload = LoadIdentityVault(path, password);
    if (!payload) return std::nullopt;
    auto material = Parse(*payload);
    crypto::CleanseMemory(payload->data(), payload->size());
    return material;
}

} // namespace cybou
