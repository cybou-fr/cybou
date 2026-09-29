// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_MAIL_SERVICE_H
#define CYBOU_MAIL_SERVICE_H

#include <cybou/account_id.h>
#include <cybou/keystore.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <cybou/signing.h>
#include <uint256.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cybou {

inline constexpr std::array<unsigned char, 5> MAILBOX_MAGIC{'C', 'Y', 'B', 'M', '1'};
inline constexpr uint32_t MAILBOX_FILE_VERSION{3};
inline constexpr uint8_t PROTECTED_MAIL_VERSION{1};
inline constexpr size_t MAX_PROTECTED_MAIL_SUBJECT_BYTES{256};
inline constexpr size_t MAX_PROTECTED_MAIL_BODY_BYTES{48000};

enum class MailFolder : uint8_t {
    INBOX = 0,
    SENT = 1,
    DRAFTS = 2,
};

enum class MailDeliveryStatus : uint8_t {
    DRAFT = 0,
    PENDING = 1,
    DELIVERED = 2,
};

/**
 * Protected text email payload (doc 49).
 * E2E-encrypted before storage or delivery. Plaintext never enters consensus.
 */
struct ProtectedMail {
    uint8_t version{PROTECTED_MAIL_VERSION};
    AccountId sender;
    AccountId recipient;
    uint64_t timestamp{0};
    std::string subject;
    std::string body;

    std::optional<std::vector<unsigned char>> Serialize() const;
    static std::optional<ProtectedMail> Deserialize(std::span<const unsigned char> bytes);

    friend bool operator==(const ProtectedMail&, const ProtectedMail&) = default;
};

/**
 * Local client mailbox item.
 * The client owns Inbox, Sent, and Draft folders and read-state (AGENTS.md).
 */
struct MailItem {
    uint256 mail_id;
    MailFolder folder{MailFolder::INBOX};
    AccountId sender;
    AccountId recipient;
    std::string subject;
    std::string body;
    uint64_t timestamp{0};
    bool read{false};
    MailDeliveryStatus delivery{MailDeliveryStatus::DRAFT};
    uint256 salt{};
    uint256 content_commitment{};

    friend bool operator==(const MailItem&, const MailItem&) = default;
};

enum class SendMailError : uint8_t {
    NONE = 0,
    NO_IDENTITY,
    SELF_MAIL,
    INVALID_RECIPIENT,
    RECIPIENT_NOT_FOUND,
    SENDER_NOT_FOUND,
    INSUFFICIENT_SYSTEM_BALANCE,
    RATE_LIMIT_EXCEEDED,
    OVERSIZED,
    CRYPTO_FAILURE,
    SUBMIT_FAILED,
    RUNTIME_ERROR,
};

struct SendMailResult {
    SendMailError error{SendMailError::NONE};
    uint256 mail_id{};
    std::string error_message{};

    explicit operator bool() const { return error == SendMailError::NONE; }
};

uint256 ComputeMailContentCommitment(const uint256& salt, std::span<const unsigned char> plaintext);

/**
 * CybouMailService manages local mailbox indexes (Inbox, Sent, Drafts).
 * Encrypted Object Storage transport and mailbox
 * synchronization are separate services and are not yet connected.
 */
class CybouMailService {
public:
    explicit CybouMailService(
        CybouNodeRuntime& runtime,
        CybouKeyStore& keystore,
        std::filesystem::path mailbox_path);
    ~CybouMailService() = default;

    CybouMailService(const CybouMailService&) = delete;
    CybouMailService& operator=(const CybouMailService&) = delete;

    /** Load local mailbox index from disk. */
    bool LoadMailbox();

    /** Persist local mailbox index to disk atomically. */
    bool SaveMailbox() const;

    /** Retrieve messages in a given folder. */
    std::vector<MailItem> GetMessages(MailFolder folder) const;

    /** Retrieve message by ID. */
    std::optional<MailItem> GetMessage(const uint256& mail_id) const;

    /** Mark a message as read/unread. */
    bool MarkAsRead(const uint256& mail_id, bool read);

    /** Save or update a local draft. Returns mail_id. */
    uint256 SaveDraft(const std::string& recipient_hex, const std::string& subject, const std::string& body, const std::optional<uint256>& existing_id = std::nullopt);

    /** Delete a draft or message by mail_id. */
    bool DeleteMessage(const uint256& mail_id);

    /** Unread count in Inbox. */
    size_t GetUnreadCount() const;

    /** Refuses submission until encrypted Object Storage delivery is integrated. */
    SendMailResult SendMail(
        const AccountId& recipient,
        const std::string& subject,
        const std::string& body);

    /** Synchronize local indexes from Object Storage (not connected yet). */
    size_t SyncMailbox();

private:
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    const std::filesystem::path m_mailbox_path;
    std::vector<MailItem> m_messages;
    mutable std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_MAIL_SERVICE_H
