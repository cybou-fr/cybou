// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

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

struct MailAttachment {
    PrivateItemId attachment_id{};
    std::string filename;
    std::uint64_t logical_size{0};
    std::optional<std::string> media_type;
    ChunkId root_chunk_id{};
    ContentKey content_key{};
    bool operator==(const MailAttachment&) const = default;
};

struct MailMessage {
    PrivateItemId message_id{};
    std::optional<PrivateItemId> reply_to_message_id;
    AccountId recipient_account_id;
    std::uint64_t client_timestamp_ms{0};
    std::string subject;
    std::string body;
    std::vector<MailAttachment> attachments;
    bool operator==(const MailMessage&) const = default;
};

enum class FileMutationKind : std::uint8_t { UPSERT_ITEM = 1, DELETE_ITEM = 2 };
enum class FileItemKind : std::uint8_t { FILE = 1, FOLDER = 2 };

struct FileItem {
    PrivateItemId item_id{};
    std::optional<PrivateItemId> parent_id; // null is root; FilesTrashParent() is Trash
    FileItemKind kind{FileItemKind::FILE};
    std::string name;
    std::uint64_t logical_size{0};
    /** A FILE always has content (an empty file too); a FOLDER never has. */
    std::optional<ChunkId> root_chunk_id;
    std::optional<ContentKey> content_key;
    /** Private client time of the last content/creation change, Unix ms. */
    std::uint64_t modified_ms{0};
    bool operator==(const FileItem&) const = default;
};

/** Reserved Files parent that denotes Trash; never valid as an item ID. */
constexpr PrivateItemId FilesTrashParent()
{
    PrivateItemId trash{};
    for (auto& byte : trash) byte = 0xff;
    return trash;
}

struct FileMutation {
    FileMutationKind kind{FileMutationKind::UPSERT_ITEM};
    PrivateItemId item_id{};
    std::optional<FileItem> item;
    bool operator==(const FileMutation&) const = default;
};

struct FilesMutationBatch {
    std::vector<FileMutation> mutations;
    bool operator==(const FilesMutationBatch&) const = default;
};

struct HistoricalKemSeed {
    std::uint64_t key_epoch{0};
    XWingSeed seed{};
    bool operator==(const HistoricalKemSeed&) const = default;
};

struct IdentityRecoveryBridge {
    AccountId account_id;
    std::uint64_t next_key_epoch{0};
    std::vector<HistoricalKemSeed> historical_seeds;
    bool operator==(const IdentityRecoveryBridge&) const = default;
};

using PrivateApplicationDocument = std::variant<MailMessage, FilesMutationBatch, IdentityRecoveryBridge>;

/** Strict v1 canonical-CBOR codecs for encrypted application ROOT metadata. */
std::optional<std::vector<unsigned char>> EncodePrivateApplicationDocument(
    const PrivateApplicationDocument& document);
std::optional<PrivateApplicationDocument> DecodePrivateApplicationDocument(
    std::span<const unsigned char> encoded);

} // namespace cybou

#endif // CYBOU_PRIVATE_APPLICATION_SCHEMA_H
