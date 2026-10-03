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
    /// Стабильный AccountID; не секретен по смыслу протокола, но хранится вместе с recovery entropy.
    std::array<unsigned char, 32> account_id{};
    /// Recovery entropy, из которой выводятся Recovery/Authorization/KEM роли.
    RecoveryEntropy recovery_entropy{};

    IdentityMaterial() = default;
    IdentityMaterial(const IdentityMaterial&) = delete;
    IdentityMaterial& operator=(const IdentityMaterial&) = delete;
    IdentityMaterial(IdentityMaterial&& other) noexcept;
    IdentityMaterial& operator=(IdentityMaterial&& other) noexcept;
    ~IdentityMaterial();
    /// Зануляет AccountID и recovery entropy в памяти.
    /// \post После вызова структура считается очищенной и непригодной для использования без повторной инициализации.
    void Clear() noexcept;
};

/// Генерирует новый случайный AccountID и recovery entropy.
/// \return Новый `IdentityMaterial` или `std::nullopt`, если CSPRNG/самопроверка ролей не удались.
/// \post Возвращаемый объект содержит секреты; не логировать и очистить после последнего использования.
/// \thread_safety Потокобезопасна.
std::optional<IdentityMaterial> GenerateIdentityMaterial();
/// Сериализует переносимый секретный материал Identity.
/// \param material Материал с ненулевым AccountID и валидной recovery entropy.
/// \return Полезная нагрузка для CYBV или `std::nullopt`, если материал некорректен.
/// \post Возвращаемые байты считаются секретом и должны быть очищены после шифрования.
/// \thread_safety Потокобезопасна.
std::optional<std::vector<unsigned char>> SerializeIdentityMaterial(const IdentityMaterial& material);
/// Создаёт новый зашифрованный файл материала Identity без перезаписи существующего.
/// \param path Путь к новому vault-файлу.
/// \param password Пароль шифрования; не логировать, очищается вызывающим кодом.
/// \param material Секретный материал Identity.
/// \return `true`, если новый файл записан и проверен повторным открытием; иначе `false`.
/// \pre `path` ещё не занят существующим vault.
/// \post При успехе исходный `material` не меняется.
/// \thread_safety Потокобезопасна относительно независимых путей; конкурентная запись в один и тот же файл не поддерживается.
bool SaveNewIdentityMaterial(const std::filesystem::path& path,
    std::string_view password, const IdentityMaterial& material);
/// Загружает и проверяет материал Identity из зашифрованного файла.
/// \param path Путь к CYBV vault.
/// \param password Пароль открытия; не логировать.
/// \return Загруженный материал либо `std::nullopt` при ошибке пароля/формата/проверки ролей.
/// \post Временная расшифрованная полезная нагрузка очищается внутри реализации.
/// \thread_safety Потокобезопасна относительно независимых файлов.
std::optional<IdentityMaterial> LoadIdentityMaterial(
    const std::filesystem::path& path, std::string_view password);

} // namespace cybou
#endif
