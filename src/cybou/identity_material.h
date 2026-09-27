// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_MATERIAL_H
#define CYBOU_IDENTITY_MATERIAL_H

#include <cybou/recovery_phrase.h>
#include <cybou/identity_kem.h>

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace cybou {

// Local secret material for one device. AccountID is independent of recovery,
// signing, and key-agreement secrets. This type is move-only and clears them.
struct IdentityMaterial {
    std::array<unsigned char, 32> account_id{};
    RecoveryEntropy recovery_entropy{};
    std::array<unsigned char, 32> device_secret{};
    DeviceX25519PrivateKey device_x25519_private_key{};
    MlKem768Seed device_mlkem768_seed{};

    IdentityMaterial() = default;
    IdentityMaterial(const IdentityMaterial&) = delete;
    IdentityMaterial& operator=(const IdentityMaterial&) = delete;
    IdentityMaterial(IdentityMaterial&& other) noexcept;
    IdentityMaterial& operator=(IdentityMaterial&& other) noexcept;
    ~IdentityMaterial();
    void Clear() noexcept;
};

std::optional<IdentityMaterial> GenerateIdentityMaterial();
std::optional<std::vector<unsigned char>> SerializeIdentityMaterial(const IdentityMaterial& material);
bool SaveNewIdentityMaterial(const std::filesystem::path& path,
    std::string_view password, const IdentityMaterial& material);
std::optional<IdentityMaterial> LoadIdentityMaterial(
    const std::filesystem::path& path, std::string_view password);

} // namespace cybou
#endif
