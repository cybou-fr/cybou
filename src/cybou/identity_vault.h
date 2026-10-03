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
/// \param password Пароль контейнера; не логировать и очищать у вызывающего кода.
/// \param payload Секретная полезная нагрузка.
/// \return Шифрованный envelope или `std::nullopt` при ошибке KDF/AEAD/валидации длины.
/// \pre `payload` не пустой и не превышает лимиты формата.
/// \post Возвращаемые байты можно сохранять на диск; plaintext внутри функции не логируется.
/// \thread_safety Потокобезопасна.
std::optional<std::vector<unsigned char>> SealIdentityVault(
    std::string_view password, std::span<const unsigned char> payload);
/// Открывает и проверяет переносимый контейнер CYBV.
/// \param password Пароль контейнера; не логировать.
/// \param envelope Содержимое CYBV.
/// \return Расшифрованная полезная нагрузка или `std::nullopt` при ошибке пароля/формата/аутентификации.
/// \post Возвращаемая полезная нагрузка считается секретом и должна быть очищена вызывающим кодом.
/// \thread_safety Потокобезопасна.
std::optional<std::vector<unsigned char>> OpenIdentityVault(
    std::string_view password, std::span<const unsigned char> envelope);

/// Создаёт новый vault без замены существующего и переоткрывает его для проверки.
/// \return `true`, если новый файл успешно опубликован и аутентифицирован повторным чтением.
/// \pre Файл по `path` ещё не существует.
/// \post При сбое файл не должен оставаться частично записанным.
bool SaveNewIdentityVault(const std::filesystem::path& path,
    std::string_view password, std::span<const unsigned char> payload);
/// Атомарно продвигает durable candidate vault поверх активного vault.
/// \return `true`, если candidate проходит проверку и становится активным.
/// \pre `candidate_path` и `active_path` указывают на один и тот же логический vault domain.
/// \post Продвижение fail-closed: при несовпадении payload или ошибке файловой системы активный vault не меняется.
bool PromoteIdentityVault(const std::filesystem::path& candidate_path,
    const std::filesystem::path& active_path, std::string_view password,
    std::span<const unsigned char> expected_payload);
/// Заменяет существующий vault только при совпадении ожидаемой текущей нагрузки.
/// \return `true`, если текущее содержимое аутентично совпало с `expected_payload` и было заменено новым.
/// \post При неудаче старый vault сохраняется без изменений.
bool ReplaceIdentityVault(const std::filesystem::path& path,
    std::string_view password, std::span<const unsigned char> expected_payload,
    std::span<const unsigned char> replacement_payload);
/// Загружает и аутентифицированно открывает vault с диска.
/// \return Расшифрованная полезная нагрузка или `std::nullopt` при ошибке чтения/формата/пароля.
/// \post Внутренние временные буферы по возможности очищаются; возвращённые байты очищает вызывающий код.
std::optional<std::vector<unsigned char>> LoadIdentityVault(
    const std::filesystem::path& path, std::string_view password);

} // namespace cybou
#endif
