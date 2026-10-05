// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Канонические приватные документы Mail/Files/RecoveryBridge и их бинарные кодеки.

#ifndef CYBOU_PRIVATE_APPLICATION_SCHEMA_H
#define CYBOU_PRIVATE_APPLICATION_SCHEMA_H

#include <cybou/account_id.h>
#include <cybou/chunk_id.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/identity_kem.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace cybou {

using PrivateItemId = std::array<unsigned char, 32>;

/// Вложение письма, опубликованное как отдельное приватное дерево контента.
struct MailAttachment {
    /// Стабильный идентификатор вложения внутри приватного документа.
    PrivateItemId attachment_id{};
    /// Имя файла, видимое приложению после расшифрования.
    std::string filename;
    /// Логический размер plaintext-файла в байтах.
    std::uint64_t logical_size{0};
    /// Опциональный media type для UX.
    std::optional<std::string> media_type;
    /// Root chunk дерева зашифрованного содержимого вложения.
    ChunkId root_chunk_id{};
    /// ContentKey, открывающий дерево вложения.
    ContentKey content_key{};
    bool operator==(const MailAttachment&) const = default;
};

/// Каноническое приватное тело письма.
struct MailMessage {
    /// Стабильный идентификатор письма.
    PrivateItemId message_id{};
    /// Идентификатор письма, на которое дан ответ.
    std::optional<PrivateItemId> reply_to_message_id;
    /// Канонический получатель письма.
    AccountId recipient_account_id;
    /// Клиентская отметка времени создания письма, Unix ms.
    std::uint64_t client_timestamp_ms{0};
    /// Тема письма.
    std::string subject;
    /// Текстовое тело письма.
    std::string body;
    /// Отдельные опубликованные вложения.
    std::vector<MailAttachment> attachments;
    bool operator==(const MailMessage&) const = default;
};

/// Вид мутации приватного каталога Files.
enum class FileMutationKind : std::uint8_t {
    /// Создать новый объект или заменить текущую запись.
    UPSERT_ITEM = 1,
    /// Логически удалить объект из каталога.
    DELETE_ITEM = 2
};
/// Тип объекта в приватном каталоге Files.
enum class FileItemKind : std::uint8_t {
    /// Файл с деревом зашифрованного содержимого.
    FILE = 1,
    /// Папка без собственного дерева контента.
    FOLDER = 2
};

/// Один элемент приватного каталога Files.
struct FileItem {
    /// Стабильный идентификатор объекта.
    PrivateItemId item_id{};
    std::optional<PrivateItemId> parent_id; // null is root; FilesTrashParent() is Trash
    /// Вид объекта: файл или папка.
    FileItemKind kind{FileItemKind::FILE};
    /// Локально расшифрованное имя.
    std::string name;
    /// Логический размер plaintext-содержимого.
    std::uint64_t logical_size{0};
    /// FILE всегда ссылается на дерево контента, даже если логически пуст.
    std::optional<ChunkId> root_chunk_id;
    std::optional<ContentKey> content_key;
    /// Приватное клиентское время последнего изменения содержимого/создания, Unix ms.
    std::uint64_t modified_ms{0};
    bool operator==(const FileItem&) const = default;
};

/// Возвращает зарезервированный parent-id корзины Files.
/// \return Специальный `PrivateItemId` из всех `0xff`, используемый только как локальная корзина.
constexpr PrivateItemId FilesTrashParent()
{
    PrivateItemId trash{};
    for (auto& byte : trash) byte = 0xff;
    return trash;
}

/// Одна мутация приватного каталога Files.
struct FileMutation {
    /// Вид изменения.
    FileMutationKind kind{FileMutationKind::UPSERT_ITEM};
    /// Идентификатор изменяемого объекта.
    PrivateItemId item_id{};
    /// Новое состояние объекта для `UPSERT_ITEM`; `std::nullopt` для `DELETE_ITEM`.
    std::optional<FileItem> item;
    bool operator==(const FileMutation&) const = default;
};

/// Канонический пакет мутаций Files одной публикации.
struct FilesMutationBatch {
    /// Упорядоченный набор мутаций одной финализированной публикации.
    std::vector<FileMutation> mutations;
    bool operator==(const FilesMutationBatch&) const = default;
};

/// Исторический KEM seed одного предыдущего epoch.
struct HistoricalKemSeed {
    /// Исторический key epoch.
    std::uint64_t key_epoch{0};
    /// KEM seed этого epoch; секрет и не должен логироваться.
    XWingSeed seed{};
    bool operator==(const HistoricalKemSeed&) const = default;
};

/// Приватный мост восстановления исторических KEM epoch.
struct IdentityRecoveryBridge {
    /// Identity, которой принадлежит мост восстановления.
    AccountId account_id;
    /// Следующий key epoch после последнего вложенного historical seed.
    std::uint64_t next_key_epoch{0};
    /// Строго возрастающий список исторических KEM seed.
    std::vector<HistoricalKemSeed> historical_seeds;
    bool operator==(const IdentityRecoveryBridge&) const = default;
};

/// Один из канонических приватных документов приложения.
using PrivateApplicationDocument = std::variant<MailMessage, FilesMutationBatch, IdentityRecoveryBridge>;

/// Кодирует приватный документ в строгий канонический бинарный формат.
/// \return Байты документа или `std::nullopt`, если документ нарушает инварианты схемы.
std::optional<std::vector<unsigned char>> EncodePrivateApplicationDocument(
    const PrivateApplicationDocument& document);
/// Декодирует и валидирует канонический приватный документ.
/// \return Документ или `std::nullopt`, если формат/лимиты/инварианты нарушены.
std::optional<PrivateApplicationDocument> DecodePrivateApplicationDocument(
    std::span<const unsigned char> encoded);

} // namespace cybou

#endif // CYBOU_PRIVATE_APPLICATION_SCHEMA_H
