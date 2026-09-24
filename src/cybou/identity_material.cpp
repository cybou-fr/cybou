// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_material.h>
#include <cybou/identity_vault.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <vector>

namespace cybou {
namespace {
constexpr std::array<unsigned char, 5> MAGIC{'C', 'V', 'I', 'D', '2'};
constexpr size_t PAYLOAD_SIZE{MAGIC.size() + 32 + 32 + 32};

bool Nonzero(const std::array<unsigned char, 32>& value)
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
    if (!Nonzero(material.account_id) || !Nonzero(material.device_secret)) return std::nullopt;
    return material;
}
} // namespace

IdentityMaterial::IdentityMaterial(IdentityMaterial&& other) noexcept
    : account_id{other.account_id}, recovery_entropy{other.recovery_entropy},
      device_secret{other.device_secret}
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
        other.Clear();
    }
    return *this;
}

IdentityMaterial::~IdentityMaterial() { Clear(); }

void IdentityMaterial::Clear() noexcept
{
    OPENSSL_cleanse(account_id.data(), account_id.size());
    OPENSSL_cleanse(recovery_entropy.data(), recovery_entropy.size());
    OPENSSL_cleanse(device_secret.data(), device_secret.size());
}

std::optional<IdentityMaterial> GenerateIdentityMaterial()
{
    IdentityMaterial material;
    auto entropy = GenerateRecoveryEntropy();
    if (!entropy || RAND_bytes(material.account_id.data(), material.account_id.size()) != 1 ||
        RAND_bytes(material.device_secret.data(), material.device_secret.size()) != 1 ||
        !Nonzero(material.account_id) || !Nonzero(material.device_secret)) return std::nullopt;
    material.recovery_entropy = *entropy;
    OPENSSL_cleanse(entropy->data(), entropy->size());
    return material;
}

bool SaveNewIdentityMaterial(const std::filesystem::path& path,
    std::string_view password, const IdentityMaterial& material)
{
    if (!Nonzero(material.account_id) || !Nonzero(material.device_secret)) return false;
    std::array<unsigned char, PAYLOAD_SIZE> payload{};
    std::copy(MAGIC.begin(), MAGIC.end(), payload.begin());
    std::copy(material.account_id.begin(), material.account_id.end(), payload.begin() + 5);
    std::copy(material.recovery_entropy.begin(), material.recovery_entropy.end(), payload.begin() + 37);
    std::copy(material.device_secret.begin(), material.device_secret.end(), payload.begin() + 69);
    const bool saved = SaveNewIdentityVault(path, password, payload);
    OPENSSL_cleanse(payload.data(), payload.size());
    return saved;
}

std::optional<IdentityMaterial> LoadIdentityMaterial(
    const std::filesystem::path& path, std::string_view password)
{
    auto payload = LoadIdentityVault(path, password);
    if (!payload) return std::nullopt;
    auto material = Parse(*payload);
    OPENSSL_cleanse(payload->data(), payload->size());
    return material;
}

} // namespace cybou
