// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Переносимый зашифрованный контейнер CYBV для локального секрета Identity.

#ifndef CYBOU_IDENTITY_VAULT_H
#define CYBOU_IDENTITY_VAULT_H

#include <optional>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

/// Запечатывает полезную нагрузку в переносимый контейнер CYBV.
std::optional<std::vector<unsigned char>> SealIdentityVault(
    std::string_view password, std::span<const unsigned char> payload);
/// Открывает и проверяет переносимый контейнер CYBV.
std::optional<std::vector<unsigned char>> OpenIdentityVault(
    std::string_view password, std::span<const unsigned char> envelope);

/// Создаёт новый vault без замены существующего и переоткрывает его для проверки.
bool SaveNewIdentityVault(const std::filesystem::path& path,
    std::string_view password, std::span<const unsigned char> payload);
/// Атомарно продвигает durable candidate vault поверх активного vault.
bool PromoteIdentityVault(const std::filesystem::path& candidate_path,
    const std::filesystem::path& active_path, std::string_view password,
    std::span<const unsigned char> expected_payload);
/// Заменяет существующий vault только при совпадении ожидаемой текущей нагрузки.
bool ReplaceIdentityVault(const std::filesystem::path& path,
    std::string_view password, std::span<const unsigned char> expected_payload,
    std::span<const unsigned char> replacement_payload);
/// Загружает и аутентифицированно открывает vault с диска.
std::optional<std::vector<unsigned char>> LoadIdentityVault(
    const std::filesystem::path& path, std::string_view password);

} // namespace cybou
#endif
