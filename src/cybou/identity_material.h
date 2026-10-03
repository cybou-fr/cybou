// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Переносимое локальное представление AccountID и recovery entropy Identity.

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

/// Переносимый секретный материал Identity для локального хранилища.
struct IdentityMaterial {
    std::array<unsigned char, 32> account_id{};
    RecoveryEntropy recovery_entropy{};

    IdentityMaterial() = default;
    IdentityMaterial(const IdentityMaterial&) = delete;
    IdentityMaterial& operator=(const IdentityMaterial&) = delete;
    IdentityMaterial(IdentityMaterial&& other) noexcept;
    IdentityMaterial& operator=(IdentityMaterial&& other) noexcept;
    ~IdentityMaterial();
    void Clear() noexcept;
};

/// Генерирует новый случайный AccountID и recovery entropy.
std::optional<IdentityMaterial> GenerateIdentityMaterial();
/// Сериализует переносимый секретный материал Identity.
std::optional<std::vector<unsigned char>> SerializeIdentityMaterial(const IdentityMaterial& material);
/// Создаёт новый зашифрованный файл материала Identity без перезаписи существующего.
bool SaveNewIdentityMaterial(const std::filesystem::path& path,
    std::string_view password, const IdentityMaterial& material);
/// Загружает и проверяет материал Identity из зашифрованного файла.
std::optional<IdentityMaterial> LoadIdentityMaterial(
    const std::filesystem::path& path, std::string_view password);

} // namespace cybou
#endif
