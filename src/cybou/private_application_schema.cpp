// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// Канонические кодеки приватных документов Mail, Files и RecoveryBridge.

#include <cybou/private_application_schema.h>
#include <cybou/binary_codec.h>
#include <algorithm>
#include <type_traits>
namespace cybou {
namespace {
// Первый байт документа однозначно выбирает schema без runtime version field и без внешнего контекста.
constexpr std::uint8_t MAIL_TYPE{1}, FILES_TYPE{2}, BRIDGE_TYPE{3};
constexpr std::size_t MAX_ATTACHMENTS{32};
constexpr std::size_t MAX_MUTATIONS{512};
constexpr std::size_t MAX_HISTORICAL_SEEDS{64};
constexpr std::size_t MAX_SUBJECT_BYTES{1024};
constexpr std::size_t MAX_BODY_BYTES{128 * 1024};
constexpr std::size_t MAX_FILENAME_BYTES{255};
constexpr std::size_t MAX_MEDIA_TYPE_BYTES{127};

template <typename T>
bool Nonzero(const T& value)
{
    return std::any_of(value.begin(), value.end(), [](const unsigned char byte) { return byte != 0; });
}

void Require(const bool condition)
{
    if (!condition) throw std::invalid_argument{"invalid private application schema"};
}

// Валидация имён и лимитов до кодирования не даёт локальному plaintext породить неоднозначный wire.
bool ValidName(const std::string_view name)
{
    return !name.empty() && name.size() <= MAX_FILENAME_BYTES && name != "." && name != ".." &&
        name.find_first_of("/\\\0", 0, 3) == std::string_view::npos;
}

bool ValidMail(const MailMessage& mail)
{
    if (!Nonzero(mail.message_id) || (mail.reply_to_message_id && !Nonzero(*mail.reply_to_message_id)) ||
        mail.recipient_account_id.IsNull() || mail.client_timestamp_ms == 0 ||
        mail.subject.size() > MAX_SUBJECT_BYTES || mail.body.size() > MAX_BODY_BYTES ||
        mail.attachments.size() > MAX_ATTACHMENTS) return false;
    for (std::size_t i{0}; i < mail.attachments.size(); ++i) {
        const auto& attachment = mail.attachments[i];
        if (!Nonzero(attachment.attachment_id) || !ValidName(attachment.filename) ||
            (attachment.media_type && (attachment.media_type->empty() ||
                attachment.media_type->size() > MAX_MEDIA_TYPE_BYTES)) ||
            !Nonzero(attachment.root_chunk_id) || !Nonzero(attachment.content_key)) return false;
        for (std::size_t j{0}; j < i; ++j) {
            if (mail.attachments[j].attachment_id == attachment.attachment_id) return false;
        }
    }
    return true;
}

bool ValidFileItem(const FileItem& item)
{
    if (!Nonzero(item.item_id) || item.item_id == FilesTrashParent() || !ValidName(item.name) ||
        item.modified_ms == 0 ||
        (item.parent_id && (!Nonzero(*item.parent_id) || *item.parent_id == item.item_id)) ||
        item.root_chunk_id.has_value() != item.content_key.has_value() ||
        (item.root_chunk_id && (!Nonzero(*item.root_chunk_id) || !Nonzero(*item.content_key)))) return false;
    if (item.kind == FileItemKind::FOLDER) {
        return item.logical_size == 0 && !item.root_chunk_id;
    }
    // Даже пустой FILE обязан ссылаться на encrypted content tree, чтобы Files не имели второго inline-plaintext режима.
    return item.kind == FileItemKind::FILE && item.root_chunk_id.has_value();
}

bool ValidFiles(const FilesMutationBatch& batch)
{
    if (batch.mutations.empty() || batch.mutations.size() > MAX_MUTATIONS) return false;
    for (const auto& mutation : batch.mutations) {
        if (!Nonzero(mutation.item_id) || mutation.item_id == FilesTrashParent()) return false;
        if (mutation.kind == FileMutationKind::UPSERT_ITEM) {
            if (!mutation.item || mutation.item->item_id != mutation.item_id || !ValidFileItem(*mutation.item)) return false;
        } else if (mutation.kind != FileMutationKind::DELETE_ITEM || mutation.item) {
            return false;
        }
    }
    return true;
}

// RecoveryBridge принимает только строго возрастающие historical epoch, чтобы restore
// не путал актуальный seed с повтором, дубликатом или локальным откатом.
bool ValidBridge(const IdentityRecoveryBridge& bridge)
{
    if (bridge.account_id.IsNull() || bridge.next_key_epoch == 0 ||
        bridge.historical_seeds.empty() || bridge.historical_seeds.size() > MAX_HISTORICAL_SEEDS) return false;
    std::optional<std::uint64_t> previous;
    for (const auto& entry : bridge.historical_seeds) {
        if (entry.key_epoch >= bridge.next_key_epoch ||
            (previous && entry.key_epoch <= *previous) || !Nonzero(entry.seed)) return false;
        previous = entry.key_epoch;
    }
    return true;
}


template<typename T> void WriteOptional(BinaryWriter& writer, const std::optional<T>& value) {
    writer.U8(value.has_value()); if (value) writer.Fixed(*value);
}
template<typename T> std::optional<T> ReadOptional(BinaryReader& reader) {
    if (!reader.Flag()) return std::nullopt; return reader.Fixed<T>();
}
void Encode(BinaryWriter& writer, const MailMessage& mail) {
    Require(ValidMail(mail)); writer.U8(MAIL_TYPE);
    writer.Fixed(mail.message_id); WriteOptional(writer, mail.reply_to_message_id);
    writer.Fixed(std::span{mail.recipient_account_id.Value().begin(), mail.recipient_account_id.Value().size()});
    writer.U64(mail.client_timestamp_ms); writer.Text(mail.subject, MAX_SUBJECT_BYTES); writer.Text(mail.body, MAX_BODY_BYTES);
    writer.U16(static_cast<std::uint16_t>(mail.attachments.size()));
    for (const auto& attachment : mail.attachments) {
        writer.Fixed(attachment.attachment_id); writer.Text(attachment.filename, MAX_FILENAME_BYTES);
        writer.U64(attachment.logical_size); writer.U8(attachment.media_type.has_value());
        if (attachment.media_type) writer.Text(*attachment.media_type, MAX_MEDIA_TYPE_BYTES);
        writer.Fixed(attachment.root_chunk_id); writer.Fixed(attachment.content_key);
    }
}
void Encode(BinaryWriter& writer, const FilesMutationBatch& batch) {
    Require(ValidFiles(batch)); writer.U8(FILES_TYPE);
    writer.U16(static_cast<std::uint16_t>(batch.mutations.size()));
    for (const auto& mutation : batch.mutations) {
        writer.U8(static_cast<std::uint8_t>(mutation.kind)); writer.Fixed(mutation.item_id);
        if (mutation.kind == FileMutationKind::DELETE_ITEM) continue;
        const auto& item = *mutation.item;
        WriteOptional(writer, item.parent_id); writer.U8(static_cast<std::uint8_t>(item.kind));
        writer.Text(item.name, MAX_FILENAME_BYTES); writer.U64(item.logical_size);
        WriteOptional(writer, item.root_chunk_id); WriteOptional(writer, item.content_key); writer.U64(item.modified_ms);
    }
}
void Encode(BinaryWriter& writer, const IdentityRecoveryBridge& bridge) {
    Require(ValidBridge(bridge)); writer.U8(BRIDGE_TYPE);
    writer.Fixed(std::span{bridge.account_id.Value().begin(), bridge.account_id.Value().size()});
    writer.U64(bridge.next_key_epoch); writer.U16(static_cast<std::uint16_t>(bridge.historical_seeds.size()));
    for (const auto& seed : bridge.historical_seeds) { writer.U64(seed.key_epoch); writer.Fixed(seed.seed); }
}
MailMessage DecodeMail(BinaryReader& reader) {
    MailMessage mail;
    mail.message_id = reader.Fixed<PrivateItemId>(); mail.reply_to_message_id = ReadOptional<PrivateItemId>(reader);
    const auto account = AccountId::FromBytes(reader.Fixed(32)); Require(account.has_value()); mail.recipient_account_id = *account;
    mail.client_timestamp_ms = reader.U64(); mail.subject = reader.Text(MAX_SUBJECT_BYTES); mail.body = reader.Text(MAX_BODY_BYTES);
    const auto count = reader.U16(); Require(count <= MAX_ATTACHMENTS);
    mail.attachments.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        MailAttachment attachment;
        attachment.attachment_id = reader.Fixed<PrivateItemId>(); attachment.filename = reader.Text(MAX_FILENAME_BYTES);
        attachment.logical_size = reader.U64(); if (reader.Flag()) attachment.media_type = reader.Text(MAX_MEDIA_TYPE_BYTES);
        attachment.root_chunk_id = reader.Fixed<ChunkId>(); attachment.content_key = reader.Fixed<ContentKey>();
        mail.attachments.push_back(std::move(attachment));
    }
    Require(ValidMail(mail)); return mail;
}
FilesMutationBatch DecodeFiles(BinaryReader& reader) {
    FilesMutationBatch batch;
    const auto count = reader.U16(); Require(count > 0 && count <= MAX_MUTATIONS);
    batch.mutations.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        FileMutation mutation;
        mutation.kind = static_cast<FileMutationKind>(reader.U8()); mutation.item_id = reader.Fixed<PrivateItemId>();
        if (mutation.kind == FileMutationKind::UPSERT_ITEM) {
            FileItem item; item.item_id = mutation.item_id; item.parent_id = ReadOptional<PrivateItemId>(reader);
            item.kind = static_cast<FileItemKind>(reader.U8()); item.name = reader.Text(MAX_FILENAME_BYTES);
            item.logical_size = reader.U64(); item.root_chunk_id = ReadOptional<ChunkId>(reader);
            item.content_key = ReadOptional<ContentKey>(reader); item.modified_ms = reader.U64(); mutation.item = std::move(item);
        } else Require(mutation.kind == FileMutationKind::DELETE_ITEM);
        batch.mutations.push_back(std::move(mutation));
    }
    Require(ValidFiles(batch)); return batch;
}
IdentityRecoveryBridge DecodeBridge(BinaryReader& reader) {
    IdentityRecoveryBridge bridge;
    const auto account = AccountId::FromBytes(reader.Fixed(32)); Require(account.has_value()); bridge.account_id = *account;
    bridge.next_key_epoch = reader.U64(); const auto count = reader.U16(); Require(count > 0 && count <= MAX_HISTORICAL_SEEDS);
    bridge.historical_seeds.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) { const auto epoch = reader.U64(); bridge.historical_seeds.push_back({epoch, reader.Fixed<XWingSeed>()}); }
    Require(ValidBridge(bridge)); return bridge;
}
}
std::optional<std::vector<unsigned char>> EncodePrivateApplicationDocument(const PrivateApplicationDocument& document) {
    try { BinaryWriter writer; std::visit([&](const auto& item) { Encode(writer, item); }, document); return writer.Take(); }
    catch (...) { return std::nullopt; }
}
std::optional<PrivateApplicationDocument> DecodePrivateApplicationDocument(std::span<const unsigned char> encoded) {
    try {
        BinaryReader reader{encoded}; const auto type = reader.U8();
        PrivateApplicationDocument document;
        switch (type) {
        case MAIL_TYPE: document = DecodeMail(reader); break;
        case FILES_TYPE: document = DecodeFiles(reader); break;
        case BRIDGE_TYPE: document = DecodeBridge(reader); break;
        default: return std::nullopt;
        }
        reader.Finish(); return document;
    } catch (...) { return std::nullopt; }
}
}
