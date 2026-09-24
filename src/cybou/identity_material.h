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

// Local secret material for the initial V2 device. AccountID is random and
// independent of both secrets. This type is move-only and clears its arrays.
struct IdentityMaterialV2 {
    std::array<unsigned char, 32> account_id{};
    RecoveryEntropy recovery_entropy{};
    std::array<unsigned char, 32> device_secret{};

    IdentityMaterialV2() = default;
    IdentityMaterialV2(const IdentityMaterialV2&) = delete;
    IdentityMaterialV2& operator=(const IdentityMaterialV2&) = delete;
    IdentityMaterialV2(IdentityMaterialV2&& other) noexcept;
    IdentityMaterialV2& operator=(IdentityMaterialV2&& other) noexcept;
    ~IdentityMaterialV2();
    void Clear() noexcept;
};

std::optional<IdentityMaterialV2> GenerateIdentityMaterialV2();
bool SaveNewIdentityMaterialV2(const std::filesystem::path& path,
    std::string_view password, const IdentityMaterialV2& material);
std::optional<IdentityMaterialV2> LoadIdentityMaterialV2(
    const std::filesystem::path& path, std::string_view password);

} // namespace cybou
#endif
