// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/private_application_schema.h>

#include <cybou/canonical_cbor.h>
#include <cybou/crypto/cleanse.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace cybou {
namespace {

constexpr std::uint64_t SCHEMA_VERSION{1};
constexpr std::uint64_t MAIL_TYPE{1};
constexpr std::uint64_t FILES_TYPE{2};
constexpr std::uint64_t BRIDGE_TYPE{3};
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

void CleanseCbor(CborValue& value)
{
    if (auto* bytes = std::get_if<CborValue::ByteString>(&value.value)) {
        crypto::CleanseMemory(bytes->data(), bytes->size());
    } else if (auto* text = std::get_if<std::string>(&value.value)) {
        crypto::CleanseMemory(text->data(), text->size());
    } else if (auto* array = std::get_if<CborValue::Array>(&value.value)) {
        for (auto& entry : *array) CleanseCbor(entry);
    } else if (auto* map = std::get_if<CborValue::Map>(&value.value)) {
        for (auto& [key, entry] : *map) {
            CleanseCbor(key);
            CleanseCbor(entry);
        }
    }
}

struct CborCleaner {
    CborValue& value;
    ~CborCleaner() { CleanseCbor(value); }
};

const CborValue::Array& Array(const CborValue& value, const std::size_t expected)
{
    const auto* array = std::get_if<CborValue::Array>(&value.value);
    Require(array && array->size() == expected);
    return *array;
}

std::uint64_t Unsigned(const CborValue& value)
{
    const auto* number = std::get_if<std::uint64_t>(&value.value);
    Require(number != nullptr);
    return *number;
}

std::string Text(const CborValue& value, const std::size_t max, const bool nonempty = false)
{
    const auto* text = std::get_if<std::string>(&value.value);
    Require(text && text->size() <= max && (!nonempty || !text->empty()));
    return *text;
}

template <typename T>
T FixedBytes(const CborValue& value, const bool nonzero = true)
{
    const auto* bytes = std::get_if<CborValue::ByteString>(&value.value);
    Require(bytes && bytes->size() == T{}.size());
    T result{};
    std::copy(bytes->begin(), bytes->end(), result.begin());
    Require(!nonzero || Nonzero(result));
    return result;
}

template <typename T>
CborValue Bytes(const T& value)
{
    return CborValue::Bytes({value.begin(), value.end()});
}

template <typename T>
CborValue OptionalBytes(const std::optional<T>& value)
{
    return value ? Bytes(*value) : CborValue::Null();
}

template <typename T>
std::optional<T> ParseOptionalBytes(const CborValue& value)
{
    if (std::holds_alternative<std::monostate>(value.value)) return std::nullopt;
    return FixedBytes<T>(value);
}

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
    if (!Nonzero(item.item_id) || !ValidName(item.name) ||
        (item.parent_id && (!Nonzero(*item.parent_id) || *item.parent_id == item.item_id)) ||
        item.root_chunk_id.has_value() != item.content_key.has_value() ||
        (item.root_chunk_id && (!Nonzero(*item.root_chunk_id) || !Nonzero(*item.content_key)))) return false;
    if (item.kind == FileItemKind::FOLDER) {
        return item.logical_size == 0 && !item.root_chunk_id;
    }
    return item.kind == FileItemKind::FILE && (item.root_chunk_id || item.logical_size == 0);
}

bool ValidFiles(const FilesMutationBatch& batch)
{
    if (batch.mutations.empty() || batch.mutations.size() > MAX_MUTATIONS) return false;
    for (const auto& mutation : batch.mutations) {
        if (!Nonzero(mutation.item_id)) return false;
        if (mutation.kind == FileMutationKind::UPSERT_ITEM) {
            if (!mutation.item || mutation.item->item_id != mutation.item_id || !ValidFileItem(*mutation.item)) return false;
        } else if (mutation.kind != FileMutationKind::DELETE_ITEM || mutation.item) {
            return false;
        }
    }
    return true;
}

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

CborValue EncodeMail(const MailMessage& mail)
{
    Require(ValidMail(mail));
    CborValue::Array attachments;
    attachments.reserve(mail.attachments.size());
    for (const auto& attachment : mail.attachments) {
        attachments.push_back(CborValue::ArrayValue({
            Bytes(attachment.attachment_id), CborValue::Text(attachment.filename),
            CborValue::Unsigned(attachment.logical_size),
            attachment.media_type ? CborValue::Text(*attachment.media_type) : CborValue::Null(),
            Bytes(attachment.root_chunk_id), Bytes(attachment.content_key),
        }));
    }
    return CborValue::ArrayValue({
        CborValue::Unsigned(MAIL_TYPE), CborValue::Unsigned(SCHEMA_VERSION),
        Bytes(mail.message_id), OptionalBytes(mail.reply_to_message_id),
        Bytes(mail.recipient_account_id.Value()), CborValue::Unsigned(mail.client_timestamp_ms),
        CborValue::Text(mail.subject), CborValue::Text(mail.body),
        CborValue::ArrayValue(std::move(attachments)),
    });
}

CborValue EncodeFiles(const FilesMutationBatch& batch)
{
    Require(ValidFiles(batch));
    CborValue::Array mutations;
    mutations.reserve(batch.mutations.size());
    for (const auto& mutation : batch.mutations) {
        if (mutation.kind == FileMutationKind::DELETE_ITEM) {
            mutations.push_back(CborValue::ArrayValue({
                CborValue::Unsigned(2), Bytes(mutation.item_id),
            }));
        } else {
            const auto& item = *mutation.item;
            mutations.push_back(CborValue::ArrayValue({
                CborValue::Unsigned(1), Bytes(item.item_id), OptionalBytes(item.parent_id),
                CborValue::Unsigned(static_cast<std::uint8_t>(item.kind)), CborValue::Text(item.name),
                CborValue::Unsigned(item.logical_size), OptionalBytes(item.root_chunk_id),
                OptionalBytes(item.content_key),
            }));
        }
    }
    return CborValue::ArrayValue({
        CborValue::Unsigned(FILES_TYPE), CborValue::Unsigned(SCHEMA_VERSION),
        CborValue::ArrayValue(std::move(mutations)),
    });
}

CborValue EncodeBridge(const IdentityRecoveryBridge& bridge)
{
    Require(ValidBridge(bridge));
    CborValue::Array seeds;
    seeds.reserve(bridge.historical_seeds.size());
    for (const auto& entry : bridge.historical_seeds) {
        seeds.push_back(CborValue::ArrayValue({CborValue::Unsigned(entry.key_epoch), Bytes(entry.seed)}));
    }
    return CborValue::ArrayValue({
        CborValue::Unsigned(BRIDGE_TYPE), CborValue::Unsigned(SCHEMA_VERSION),
        Bytes(bridge.account_id.Value()), CborValue::Unsigned(bridge.next_key_epoch),
        CborValue::ArrayValue(std::move(seeds)),
    });
}

MailMessage DecodeMail(const CborValue& root)
{
    const auto& fields = Array(root, 9);
    MailMessage mail;
    mail.message_id = FixedBytes<PrivateItemId>(fields[2]);
    mail.reply_to_message_id = ParseOptionalBytes<PrivateItemId>(fields[3]);
    const auto account_bytes = FixedBytes<PrivateItemId>(fields[4]);
    mail.recipient_account_id = *AccountId::FromBytes(account_bytes);
    mail.client_timestamp_ms = Unsigned(fields[5]);
    mail.subject = Text(fields[6], MAX_SUBJECT_BYTES);
    mail.body = Text(fields[7], MAX_BODY_BYTES);
    const auto* attachments = std::get_if<CborValue::Array>(&fields[8].value);
    Require(attachments && attachments->size() <= MAX_ATTACHMENTS);
    for (const auto& encoded : *attachments) {
        const auto& entry = Array(encoded, 6);
        MailAttachment attachment;
        attachment.attachment_id = FixedBytes<PrivateItemId>(entry[0]);
        attachment.filename = Text(entry[1], MAX_FILENAME_BYTES, true);
        attachment.logical_size = Unsigned(entry[2]);
        if (!std::holds_alternative<std::monostate>(entry[3].value)) {
            attachment.media_type = Text(entry[3], MAX_MEDIA_TYPE_BYTES, true);
        }
        attachment.root_chunk_id = FixedBytes<ChunkId>(entry[4]);
        attachment.content_key = FixedBytes<ContentKey>(entry[5]);
        mail.attachments.push_back(std::move(attachment));
    }
    Require(ValidMail(mail));
    return mail;
}

FilesMutationBatch DecodeFiles(const CborValue& root)
{
    const auto& fields = Array(root, 3);
    const auto* entries = std::get_if<CborValue::Array>(&fields[2].value);
    Require(entries && entries->size() <= MAX_MUTATIONS);
    FilesMutationBatch batch;
    for (const auto& encoded : *entries) {
        const auto* array = std::get_if<CborValue::Array>(&encoded.value);
        Require(array && !array->empty());
        FileMutation mutation;
        const auto kind = Unsigned((*array)[0]);
        if (kind == 2) {
            const auto& values = Array(encoded, 2);
            mutation.kind = FileMutationKind::DELETE_ITEM;
            mutation.item_id = FixedBytes<PrivateItemId>(values[1]);
        } else {
            Require(kind == 1);
            const auto& values = Array(encoded, 8);
            mutation.item_id = FixedBytes<PrivateItemId>(values[1]);
            FileItem item;
            item.item_id = mutation.item_id;
            item.parent_id = ParseOptionalBytes<PrivateItemId>(values[2]);
            item.kind = static_cast<FileItemKind>(Unsigned(values[3]));
            item.name = Text(values[4], MAX_FILENAME_BYTES, true);
            item.logical_size = Unsigned(values[5]);
            item.root_chunk_id = ParseOptionalBytes<ChunkId>(values[6]);
            item.content_key = ParseOptionalBytes<ContentKey>(values[7]);
            mutation.item = std::move(item);
        }
        batch.mutations.push_back(std::move(mutation));
    }
    Require(ValidFiles(batch));
    return batch;
}

IdentityRecoveryBridge DecodeBridge(const CborValue& root)
{
    const auto& fields = Array(root, 5);
    IdentityRecoveryBridge bridge;
    const auto account_bytes = FixedBytes<PrivateItemId>(fields[2]);
    bridge.account_id = *AccountId::FromBytes(account_bytes);
    bridge.next_key_epoch = Unsigned(fields[3]);
    const auto* seeds = std::get_if<CborValue::Array>(&fields[4].value);
    Require(seeds && seeds->size() <= MAX_HISTORICAL_SEEDS);
    for (const auto& encoded : *seeds) {
        const auto& values = Array(encoded, 2);
        bridge.historical_seeds.push_back({Unsigned(values[0]), FixedBytes<XWingSeed>(values[1])});
    }
    Require(ValidBridge(bridge));
    return bridge;
}

} // namespace

std::optional<std::vector<unsigned char>> EncodePrivateApplicationDocument(
    const PrivateApplicationDocument& document)
{
    try {
        auto value = std::visit([](const auto& item) -> CborValue {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, MailMessage>) return EncodeMail(item);
            else if constexpr (std::is_same_v<T, FilesMutationBatch>) return EncodeFiles(item);
            else return EncodeBridge(item);
        }, document);
        CborCleaner cleanse{value};
        return EncodeCanonicalCbor(value);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<PrivateApplicationDocument> DecodePrivateApplicationDocument(
    const std::span<const unsigned char> encoded)
{
    try {
        auto value = DecodeCanonicalCbor(encoded);
        CborCleaner cleanse{value};
        const auto* fields = std::get_if<CborValue::Array>(&value.value);
        Require(fields && fields->size() >= 2 && Unsigned((*fields)[1]) == SCHEMA_VERSION);
        switch (Unsigned((*fields)[0])) {
        case MAIL_TYPE: return PrivateApplicationDocument{DecodeMail(value)};
        case FILES_TYPE: return PrivateApplicationDocument{DecodeFiles(value)};
        case BRIDGE_TYPE: return PrivateApplicationDocument{DecodeBridge(value)};
        default: return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace cybou
