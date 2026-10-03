// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Реализация переносимого локального материала Identity.

#include <cybou/identity_material.h>
#include <cybou/identity_vault.h>
#include <cybou/identity_crypto.h>
#include <cybou/crypto/cleanse.h>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <vector>

namespace cybou {
namespace {
constexpr std::array<unsigned char, 4> MAGIC{'C', 'Y', 'I', 'D'};
constexpr size_t PAYLOAD_SIZE{MAGIC.size() + 32 + 32};

template <size_t N>
bool Nonzero(const std::array<unsigned char, N>& value)
{
    return std::any_of(value.begin(), value.end(), [](unsigned char byte) { return byte != 0; });
}

// Материал считается годным только если из одной recovery entropy воспроизводятся все роли
// и KEM проходит полный self-check, иначе локальный vault не сохраняет "почти рабочий" секрет.
bool DerivedKeysValid(const RecoveryEntropy& entropy)
{
    if (!Nonzero(entropy)) return false;
    const auto root = DeriveIdentityPublicKey(entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto authorization = DeriveIdentityPublicKey(entropy, IdentityKeyPurpose::AUTHORIZATION);
    auto seed = DeriveIdentityXWingSeed(entropy);
    const bool valid = root && authorization && seed && ValidateXWingKeyPair(*seed);
    if (seed) crypto::CleanseMemory(seed->data(), seed->size());
    return valid;
}

// Формат payload минимален: stable AccountID + recovery entropy. Остальные роли
// детерминированно восстанавливаются локально и не дублируются в переносимом vault.
std::optional<IdentityMaterial> Parse(std::span<const unsigned char> bytes)
{
    if (bytes.size() != PAYLOAD_SIZE || !std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin())) return std::nullopt;
    IdentityMaterial material;
    std::copy_n(bytes.begin() + MAGIC.size(), 32, material.account_id.begin());
    std::copy_n(bytes.begin() + MAGIC.size() + 32, 32, material.recovery_entropy.begin());
    if (!Nonzero(material.account_id) || !DerivedKeysValid(material.recovery_entropy)) return std::nullopt;
    return material;
}
} // namespace

IdentityMaterial::IdentityMaterial(IdentityMaterial&& other) noexcept
    : account_id{other.account_id}, recovery_entropy{other.recovery_entropy}
{
    other.Clear();
}

IdentityMaterial& IdentityMaterial::operator=(IdentityMaterial&& other) noexcept
{
    if (this != &other) {
        Clear();
        account_id = other.account_id;
        recovery_entropy = other.recovery_entropy;
        other.Clear();
    }
    return *this;
}

IdentityMaterial::~IdentityMaterial() { Clear(); }

void IdentityMaterial::Clear() noexcept
{
    crypto::CleanseMemory(account_id.data(), account_id.size());
    crypto::CleanseMemory(recovery_entropy.data(), recovery_entropy.size());
}

std::optional<IdentityMaterial> GenerateIdentityMaterial()
{
    IdentityMaterial material;
    auto entropy = GenerateRecoveryEntropy();
    if (!entropy) return std::nullopt;
    if (RAND_bytes(material.account_id.data(), material.account_id.size()) != 1 || !Nonzero(material.account_id)) {
        crypto::CleanseMemory(entropy->data(), entropy->size());
        return std::nullopt;
    }
    material.recovery_entropy = *entropy;
    crypto::CleanseMemory(entropy->data(), entropy->size());
    if (!DerivedKeysValid(material.recovery_entropy)) return std::nullopt;
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
    if (!Nonzero(material.account_id) || !DerivedKeysValid(material.recovery_entropy)) return std::nullopt;
    std::vector<unsigned char> payload(PAYLOAD_SIZE);
    std::copy(MAGIC.begin(), MAGIC.end(), payload.begin());
    std::copy(material.account_id.begin(), material.account_id.end(), payload.begin() + MAGIC.size());
    std::copy(material.recovery_entropy.begin(), material.recovery_entropy.end(), payload.begin() + MAGIC.size() + 32);
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
