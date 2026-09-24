// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_MATERIAL_H
#define CYBOU_IDENTITY_MATERIAL_H

#include <cybou/recovery_phrase.h>

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace cybou {

// Local secret material for the initial device. AccountID is random and
// independent of both secrets. This type is move-only and clears its arrays.
struct IdentityMaterial {
    std::array<unsigned char, 32> account_id{};
    RecoveryEntropy recovery_entropy{};
    std::array<unsigned char, 32> device_secret{};

    IdentityMaterial() = default;
    IdentityMaterial(const IdentityMaterial&) = delete;
    IdentityMaterial& operator=(const IdentityMaterial&) = delete;
    IdentityMaterial(IdentityMaterial&& other) noexcept;
    IdentityMaterial& operator=(IdentityMaterial&& other) noexcept;
    ~IdentityMaterial();
    void Clear() noexcept;
};

std::optional<IdentityMaterial> GenerateIdentityMaterial();
bool SaveNewIdentityMaterial(const std::filesystem::path& path,
    std::string_view password, const IdentityMaterial& material);
std::optional<IdentityMaterial> LoadIdentityMaterial(
    const std::filesystem::path& path, std::string_view password);

} // namespace cybou
#endif
