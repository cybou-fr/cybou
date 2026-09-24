// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/mail_service.h>

#include <crypto/chacha20poly1305.h>
#include <crypto/common.h>
#include <crypto/hkdf_sha256_32.h>
#include <crypto/sha256.h>
#include <random.h>
#include <support/cleanse.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <string_view>

#ifdef WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cybou {
namespace {

inline void AppendUint32LE(std::vector<unsigned char>& out, uint32_t val)
{
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(val >> (8 * i)));
}

inline void AppendUint64LE(std::vector<unsigned char>& out, uint64_t val)
{
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(val >> (8 * i)));
}

inline uint32_t ReadUint32LE(const unsigned char* p)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= uint32_t{p[i]} << (8 * i);
    return v;
}

inline uint64_t ReadUint64LE(const unsigned char* p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t{p[i]} << (8 * i);
    return v;
}

} // namespace

std::vector<unsigned char> ProtectedMailV1::Serialize() const
{
    std::vector<unsigned char> out;
    out.push_back(version);
    const auto& s_val = sender.Value();
    out.insert(out.end(), s_val.begin(), s_val.end());
    const auto& r_val = recipient.Value();
    out.insert(out.end(), r_val.begin(), r_val.end());

    AppendUint64LE(out, timestamp);

    AppendUint32LE(out, static_cast<uint32_t>(subject.size()));
    out.insert(out.end(), subject.begin(), subject.end());

    AppendUint32LE(out, static_cast<uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());

    return out;
}

std::optional<ProtectedMailV1> ProtectedMailV1::Deserialize(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 81) return std::nullopt;
    if (bytes[0] != PROTECTED_MAIL_VERSION) return std::nullopt;

    ProtectedMailV1 mail;
    mail.version = bytes[0];

    auto s_id = AccountId::FromBytes(bytes.subspan(1, 32));
    auto r_id = AccountId::FromBytes(bytes.subspan(33, 32));
    if (!s_id || !r_id) return std::nullopt;
    mail.sender = *s_id;
    mail.recipient = *r_id;

    mail.timestamp = ReadUint64LE(bytes.data() + 65);

    size_t offset = 73;
    const uint32_t subj_len = ReadUint32LE(bytes.data() + offset);
    offset += 4;
    if (offset + subj_len > bytes.size()) return std::nullopt;
    mail.subject.assign(reinterpret_cast<const char*>(bytes.data() + offset), subj_len);
    offset += subj_len;

    if (offset + 4 > bytes.size()) return std::nullopt;
    const uint32_t body_len = ReadUint32LE(bytes.data() + offset);
    offset += 4;
    if (offset + body_len > bytes.size()) return std::nullopt;
    mail.body.assign(reinterpret_cast<const char*>(bytes.data() + offset), body_len);

    return mail;
}

std::optional<std::vector<unsigned char>> EncryptMailPayload(
    const uint256& recipient_ed25519_pubkey,
    const AccountId& sender,
    const AccountId& recipient,
    const uint256& salt,
    const ProtectedMailV1& mail)
{
    const auto recipient_x25519 = Ed25519PublicKeyToX25519(recipient_ed25519_pubkey);
    if (!recipient_x25519) return std::nullopt;

    std::array<unsigned char, 32> eph_sk{};
    uint256 eph_pk;
    if (!GenerateX25519KeyPair(eph_sk, eph_pk)) {
        return std::nullopt;
    }

    const auto shared_secret = X25519DeriveSharedSecret(eph_sk, *recipient_x25519);
    memory_cleanse(eph_sk.data(), eph_sk.size());
    if (!shared_secret) return std::nullopt;

    CHKDF_HMAC_SHA256_L32 hkdf(shared_secret->data(), shared_secret->size(), "CYBOU/MAIL_HKDF/V1");
    std::array<unsigned char, 32> cek{};
    hkdf.Expand32("CYBOU/MAIL_CEK/V1", cek.data());
    std::array<unsigned char, 32> nonce_buf{};
    hkdf.Expand32("CYBOU/MAIL_NONCE/V1", nonce_buf.data());

    AEADChaCha20Poly1305::Nonce96 nonce96{
        ReadLE32(nonce_buf.data()),
        ReadLE64(nonce_buf.data() + 4)
    };

    const auto plain_serialized = mail.Serialize();
    const uint256 commitment = ComputeMailContentCommitment(salt, plain_serialized);

    std::vector<std::byte> to_encrypt(32 + plain_serialized.size());
    std::copy(reinterpret_cast<const std::byte*>(salt.begin()),
              reinterpret_cast<const std::byte*>(salt.end()),
              to_encrypt.begin());
    std::copy(reinterpret_cast<const std::byte*>(plain_serialized.data()),
              reinterpret_cast<const std::byte*>(plain_serialized.data() + plain_serialized.size()),
              to_encrypt.begin() + 32);

    static constexpr std::string_view AAD_PREFIX{"CYBOU-MAIL-AAD-V1"};
    std::vector<std::byte> aad;
    aad.reserve(AAD_PREFIX.size() + 32 + 32 + 32);
    aad.insert(aad.end(), reinterpret_cast<const std::byte*>(AAD_PREFIX.data()),
               reinterpret_cast<const std::byte*>(AAD_PREFIX.data() + AAD_PREFIX.size()));
    aad.insert(aad.end(), reinterpret_cast<const std::byte*>(sender.Value().begin()),
               reinterpret_cast<const std::byte*>(sender.Value().end()));
    aad.insert(aad.end(), reinterpret_cast<const std::byte*>(recipient.Value().begin()),
               reinterpret_cast<const std::byte*>(recipient.Value().end()));
    aad.insert(aad.end(), reinterpret_cast<const std::byte*>(commitment.begin()),
               reinterpret_cast<const std::byte*>(commitment.end()));

    std::vector<std::byte> cipher_bytes(to_encrypt.size() + AEADChaCha20Poly1305::EXPANSION);
    AEADChaCha20Poly1305 aead(std::span<const std::byte>{reinterpret_cast<const std::byte*>(cek.data()), 32});
    aead.Encrypt(to_encrypt, aad, nonce96, cipher_bytes);

    memory_cleanse(cek.data(), cek.size());
    memory_cleanse(nonce_buf.data(), nonce_buf.size());
    memory_cleanse(to_encrypt.data(), to_encrypt.size());

    std::vector<unsigned char> outer;
    outer.reserve(1 + 32 + cipher_bytes.size());
    outer.push_back(0x01); // Suite ID: X25519_CHACHA20POLY1305
    outer.insert(outer.end(), eph_pk.begin(), eph_pk.end());
    outer.insert(outer.end(), reinterpret_cast<const unsigned char*>(cipher_bytes.data()),
                 reinterpret_cast<const unsigned char*>(cipher_bytes.data() + cipher_bytes.size()));
    return outer;
}

std::optional<std::pair<uint256, ProtectedMailV1>> DecryptMailPayload(
    const CybouKeyStore& keystore,
    const AccountId& sender,
    const AccountId& recipient,
    const uint256& content_commitment,
    std::span<const unsigned char> ciphertext)
{
    if (ciphertext.size() < 1 + 32 + 32 + 81 + AEADChaCha20Poly1305::EXPANSION) {
        return std::nullopt;
    }
    if (ciphertext[0] != 0x01) {
        return std::nullopt;
    }

    uint256 eph_pk;
    std::copy(ciphertext.begin() + 1, ciphertext.begin() + 33, eph_pk.begin());

    const auto shared_secret = keystore.DeriveX25519SharedSecret(eph_pk);
    if (!shared_secret) return std::nullopt;

    CHKDF_HMAC_SHA256_L32 hkdf(shared_secret->data(), shared_secret->size(), "CYBOU/MAIL_HKDF/V1");
    std::array<unsigned char, 32> cek{};
    hkdf.Expand32("CYBOU/MAIL_CEK/V1", cek.data());
    std::array<unsigned char, 32> nonce_buf{};
    hkdf.Expand32("CYBOU/MAIL_NONCE/V1", nonce_buf.data());

    AEADChaCha20Poly1305::Nonce96 nonce96{
        ReadLE32(nonce_buf.data()),
        ReadLE64(nonce_buf.data() + 4)
    };

    static constexpr std::string_view AAD_PREFIX{"CYBOU-MAIL-AAD-V1"};
    std::vector<std::byte> aad;
    aad.reserve(AAD_PREFIX.size() + 32 + 32 + 32);
    aad.insert(aad.end(), reinterpret_cast<const std::byte*>(AAD_PREFIX.data()),
               reinterpret_cast<const std::byte*>(AAD_PREFIX.data() + AAD_PREFIX.size()));
    aad.insert(aad.end(), reinterpret_cast<const std::byte*>(sender.Value().begin()),
               reinterpret_cast<const std::byte*>(sender.Value().end()));
    aad.insert(aad.end(), reinterpret_cast<const std::byte*>(recipient.Value().begin()),
               reinterpret_cast<const std::byte*>(recipient.Value().end()));
    aad.insert(aad.end(), reinterpret_cast<const std::byte*>(content_commitment.begin()),
               reinterpret_cast<const std::byte*>(content_commitment.end()));

    std::span<const std::byte> cipher_payload{
        reinterpret_cast<const std::byte*>(ciphertext.data() + 33),
        ciphertext.size() - 33
    };

    std::vector<std::byte> decrypted(cipher_payload.size() - AEADChaCha20Poly1305::EXPANSION);
    AEADChaCha20Poly1305 aead(std::span<const std::byte>{reinterpret_cast<const std::byte*>(cek.data()), 32});
    const bool dec_ok = aead.Decrypt(cipher_payload, aad, nonce96, decrypted);

    memory_cleanse(cek.data(), cek.size());
    memory_cleanse(nonce_buf.data(), nonce_buf.size());

    if (!dec_ok || decrypted.size() < 32 + 81) {
        return std::nullopt;
    }

    uint256 salt;
    std::copy(reinterpret_cast<const unsigned char*>(decrypted.data()),
              reinterpret_cast<const unsigned char*>(decrypted.data() + 32),
              salt.begin());

    std::span<const unsigned char> mail_bytes{
        reinterpret_cast<const unsigned char*>(decrypted.data() + 32),
        decrypted.size() - 32
    };

    const uint256 check_comm = ComputeMailContentCommitment(salt, mail_bytes);
    if (check_comm != content_commitment) {
        return std::nullopt;
    }

    const auto deserialized = ProtectedMailV1::Deserialize(mail_bytes);
    if (!deserialized) return std::nullopt;
    if (deserialized->sender != sender || deserialized->recipient != recipient) {
        return std::nullopt;
    }

    return std::make_pair(salt, *deserialized);
}

CybouMailService::CybouMailService(
    CybouNodeRuntime& runtime,
    CybouKeyStore& keystore,
    std::filesystem::path mailbox_path)
    : m_runtime{runtime},
      m_keystore{keystore},
      m_mailbox_path{std::move(mailbox_path)}
{
    LoadMailbox();
}

bool CybouMailService::LoadMailbox()
{
    std::lock_guard lock(m_mutex);
    m_messages.clear();

    if (!std::filesystem::exists(m_mailbox_path)) {
        return true;
    }

    std::ifstream in(m_mailbox_path, std::ios::binary);
    if (!in.is_open()) return false;

    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
    in.close();

    if (bytes.size() < MAILBOX_MAGIC.size() + 8) return false;
    if (!std::equal(MAILBOX_MAGIC.begin(), MAILBOX_MAGIC.end(), bytes.begin())) return false;

    size_t offset = MAILBOX_MAGIC.size();
    const uint32_t version = ReadUint32LE(bytes.data() + offset);
    offset += 4;
    if (version != MAILBOX_FILE_VERSION) return false;

    m_last_scanned_height = ReadUint64LE(bytes.data() + offset);
    offset += 8;

    if (offset + 4 > bytes.size()) return false;
    const uint32_t count = ReadUint32LE(bytes.data() + offset);
    offset += 4;

    m_messages.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (offset + 32 + 1 + 32 + 32 > bytes.size()) return false;
        MailItemV1 item;
        std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, item.mail_id.begin());
        offset += 32;

        item.folder = static_cast<MailFolder>(bytes[offset++]);

        auto s_id = AccountId::FromBytes(std::span{bytes}.subspan(offset, 32));
        offset += 32;
        auto r_id = AccountId::FromBytes(std::span{bytes}.subspan(offset, 32));
        offset += 32;
        if (!s_id || !r_id) return false;
        item.sender = *s_id;
        item.recipient = *r_id;

        if (offset + 4 > bytes.size()) return false;
        const uint32_t subj_len = ReadUint32LE(bytes.data() + offset);
        offset += 4;
        if (offset + subj_len > bytes.size()) return false;
        item.subject.assign(reinterpret_cast<const char*>(bytes.data() + offset), subj_len);
        offset += subj_len;

        if (offset + 4 > bytes.size()) return false;
        const uint32_t body_len = ReadUint32LE(bytes.data() + offset);
        offset += 4;
        if (offset + body_len > bytes.size()) return false;
        item.body.assign(reinterpret_cast<const char*>(bytes.data() + offset), body_len);
        offset += body_len;

        if (offset + 8 + 1 + 1 + 8 + 32 + 8 + 32 + 32 + 32 + 8 + 1 > bytes.size()) return false;
        item.timestamp = ReadUint64LE(bytes.data() + offset); offset += 8;
        item.read = (bytes[offset++] != 0);
        item.finality = static_cast<MailFinalityStatus>(bytes[offset++]);
        item.block_height = ReadUint64LE(bytes.data() + offset); offset += 8;
        std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, item.block_id.begin()); offset += 32;
        item.operation_index = ReadUint64LE(bytes.data() + offset); offset += 8;
        std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, item.salt.begin()); offset += 32;
        std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, item.content_commitment.begin()); offset += 32;
        std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, item.discovery_tag.begin()); offset += 32;
        item.fee = ReadUint64LE(bytes.data() + offset); offset += 8;

        const uint8_t has_evidence = bytes[offset++];
        if (has_evidence) {
            if (offset + 4 > bytes.size()) return false;
            const uint32_t ev_len = ReadUint32LE(bytes.data() + offset);
            offset += 4;
            if (offset + ev_len > bytes.size()) return false;
            item.evidence_bundle = DeserializeMailEvidenceBundle(std::span{bytes}.subspan(offset, ev_len));
            offset += ev_len;
        }

        m_messages.push_back(std::move(item));
    }

    return true;
}

bool CybouMailService::SaveMailbox() const
{
    std::vector<unsigned char> bytes;
    bytes.insert(bytes.end(), MAILBOX_MAGIC.begin(), MAILBOX_MAGIC.end());
    AppendUint32LE(bytes, MAILBOX_FILE_VERSION);
    AppendUint64LE(bytes, m_last_scanned_height);
    AppendUint32LE(bytes, static_cast<uint32_t>(m_messages.size()));

    for (const auto& item : m_messages) {
        bytes.insert(bytes.end(), item.mail_id.begin(), item.mail_id.end());
        bytes.push_back(static_cast<uint8_t>(item.folder));
        bytes.insert(bytes.end(), item.sender.Value().begin(), item.sender.Value().end());
        bytes.insert(bytes.end(), item.recipient.Value().begin(), item.recipient.Value().end());

        AppendUint32LE(bytes, static_cast<uint32_t>(item.subject.size()));
        bytes.insert(bytes.end(), item.subject.begin(), item.subject.end());

        AppendUint32LE(bytes, static_cast<uint32_t>(item.body.size()));
        bytes.insert(bytes.end(), item.body.begin(), item.body.end());

        AppendUint64LE(bytes, item.timestamp);
        bytes.push_back(item.read ? 1 : 0);
        bytes.push_back(static_cast<uint8_t>(item.finality));
        AppendUint64LE(bytes, item.block_height);
        bytes.insert(bytes.end(), item.block_id.begin(), item.block_id.end());
        AppendUint64LE(bytes, item.operation_index);
        bytes.insert(bytes.end(), item.salt.begin(), item.salt.end());
        bytes.insert(bytes.end(), item.content_commitment.begin(), item.content_commitment.end());
        bytes.insert(bytes.end(), item.discovery_tag.begin(), item.discovery_tag.end());
        AppendUint64LE(bytes, item.fee);

        if (item.evidence_bundle.has_value()) {
            bytes.push_back(1);
            const auto ev_bytes = SerializeMailEvidenceBundle(*item.evidence_bundle);
            AppendUint32LE(bytes, static_cast<uint32_t>(ev_bytes.size()));
            bytes.insert(bytes.end(), ev_bytes.begin(), ev_bytes.end());
        } else {
            bytes.push_back(0);
        }
    }

    const auto tmp_path = m_mailbox_path.string() + ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        out.flush();
        if (!out.good()) return false;
    }

#ifdef WIN32
    const std::wstring w_tmp(tmp_path.begin(), tmp_path.end());
    const std::wstring w_dst(m_mailbox_path.native());
    HANDLE hFile = CreateFileW(
        w_tmp.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
    if (std::filesystem::exists(m_mailbox_path)) {
        if (!ReplaceFileW(w_dst.c_str(), w_tmp.c_str(), nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
            if (!MoveFileExW(w_tmp.c_str(), w_dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                return false;
            }
        }
    } else {
        if (!MoveFileExW(w_tmp.c_str(), w_dst.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
            return false;
        }
    }
    return true;
#else
    int fd = open(tmp_path.c_str(), O_RDONLY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
    return (rename(tmp_path.c_str(), m_mailbox_path.c_str()) == 0);
#endif
}

std::vector<MailItemV1> CybouMailService::GetMessages(MailFolder folder) const
{
    std::lock_guard lock(m_mutex);
    std::vector<MailItemV1> out;
    for (const auto& item : m_messages) {
        if (item.folder == folder) {
            out.push_back(item);
        }
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::optional<MailItemV1> CybouMailService::GetMessage(const uint256& mail_id) const
{
    std::lock_guard lock(m_mutex);
    for (const auto& item : m_messages) {
        if (item.mail_id == mail_id) {
            return item;
        }
    }
    return std::nullopt;
}

bool CybouMailService::MarkAsRead(const uint256& mail_id, bool read)
{
    std::lock_guard lock(m_mutex);
    for (auto& item : m_messages) {
        if (item.mail_id == mail_id) {
            if (item.read != read) {
                item.read = read;
                SaveMailbox();
            }
            return true;
        }
    }
    return false;
}

uint256 CybouMailService::SaveDraft(
    const std::string& recipient_hex,
    const std::string& subject,
    const std::string& body,
    const std::optional<uint256>& existing_id)
{
    std::lock_guard lock(m_mutex);
    uint256 id;
    if (existing_id.has_value()) {
        id = *existing_id;
    } else {
        GetRandBytes(id);
    }

    AccountId rec_id;
    const auto rec_u256 = uint256::FromUserHex(recipient_hex);
    if (rec_u256.has_value() && !rec_u256->IsNull()) {
        rec_id = AccountId{*rec_u256};
    }

    const auto my_account = m_keystore.GetAccountId().value_or(AccountId{});

    for (auto& item : m_messages) {
        if (item.mail_id == id && item.folder == MailFolder::DRAFTS) {
            item.recipient = rec_id;
            item.subject = subject;
            item.body = body;
            item.timestamp = static_cast<uint64_t>(std::time(nullptr));
            SaveMailbox();
            return id;
        }
    }

    MailItemV1 item;
    item.mail_id = id;
    item.folder = MailFolder::DRAFTS;
    item.sender = my_account;
    item.recipient = rec_id;
    item.subject = subject;
    item.body = body;
    item.timestamp = static_cast<uint64_t>(std::time(nullptr));
    item.read = true;
    item.finality = MailFinalityStatus::DRAFT;

    m_messages.push_back(std::move(item));
    SaveMailbox();
    return id;
}

bool CybouMailService::DeleteMessage(const uint256& mail_id)
{
    std::lock_guard lock(m_mutex);
    const auto it = std::remove_if(m_messages.begin(), m_messages.end(),
        [&mail_id](const MailItemV1& m) { return m.mail_id == mail_id; });
    if (it != m_messages.end()) {
        m_messages.erase(it, m_messages.end());
        SaveMailbox();
        return true;
    }
    return false;
}

size_t CybouMailService::GetUnreadCount() const
{
    std::lock_guard lock(m_mutex);
    size_t count = 0;
    for (const auto& item : m_messages) {
        if (item.folder == MailFolder::INBOX && !item.read) {
            count++;
        }
    }
    return count;
}

SendMailResult CybouMailService::SendMail(
    const AccountId& recipient,
    const std::string& subject,
    const std::string& body)
{
    std::lock_guard lock(m_mutex);

    const auto sender_id = m_keystore.GetAccountId();
    if (!sender_id || sender_id->IsNull()) {
        return {.error = SendMailError::NO_IDENTITY, .error_message = "No active identity configured"};
    }
    if (recipient.IsNull() || *sender_id == recipient) {
        return {.error = SendMailError::SELF_MAIL, .error_message = "Cannot send mail to yourself"};
    }

    const auto recipient_state = m_runtime.GetAccountState(recipient);
    if (!recipient_state) {
        return {.error = SendMailError::RECIPIENT_NOT_FOUND, .error_message = "Recipient account not found on-chain"};
    }

    const auto sender_state = m_runtime.GetAccountState(*sender_id);
    if (!sender_state) {
        return {.error = SendMailError::SENDER_NOT_FOUND, .error_message = "Sender account not found on-chain"};
    }

    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;
    const uint64_t current_height = m_runtime.GetFinalizedHeight().value_or(0);
    const uint64_t current_epoch = EpochForHeight(current_height, params);
    if (sender_state->last_mail_epoch == current_epoch &&
        sender_state->mail_count_in_epoch >= CalculateMailRateLimit(params)) {
        return {.error = SendMailError::RATE_LIMIT_EXCEEDED, .error_message = "Mail quota exceeded for current epoch"};
    }

    ProtectedMailV1 mail;
    mail.version = PROTECTED_MAIL_VERSION;
    mail.sender = *sender_id;
    mail.recipient = recipient;
    mail.timestamp = static_cast<uint64_t>(std::time(nullptr));
    mail.subject = subject;
    mail.body = body;

    const auto plain_bytes = mail.Serialize();
    uint256 salt;
    GetRandBytes(salt);

    const uint256 content_commitment = ComputeMailContentCommitment(salt, plain_bytes);
    const uint256 discovery_tag = ComputeRecipientDiscoveryTag(recipient_state->active_authorization_key, salt);

    const auto encrypted = EncryptMailPayload(
        recipient_state->active_authorization_key,
        *sender_id,
        recipient,
        salt,
        mail);
    if (!encrypted) {
        return {.error = SendMailError::CRYPTO_FAILURE, .error_message = "Failed to encrypt mail payload"};
    }

    if (encrypted->size() > params.max_mail_ciphertext_size) {
        return {.error = SendMailError::OVERSIZED, .error_message = "Mail payload exceeds maximum size"};
    }

    const uint64_t fee = MailFeeForSize(encrypted->size(), params);
    if (sender_state->system_balance < fee) {
        return {.error = SendMailError::INSUFFICIENT_SYSTEM_BALANCE, .error_message = "Insufficient system balance for fee"};
    }

    MailOpV1 mail_op;
    mail_op.version = MAIL_OP_VERSION;
    mail_op.recipient = recipient;
    mail_op.content_commitment = content_commitment;
    mail_op.discovery_tag = discovery_tag;
    mail_op.ciphertext = *encrypted;

    AuthorizedOperationV1 auth_op;
    auth_op.version = AUTHORIZED_OPERATION_VERSION;
    auth_op.account_id = *sender_id;
    auth_op.nonce = sender_state->next_nonce;
    auth_op.payload = mail_op;

    const uint256 digest = ComputeUserOperationDigest(
        m_runtime.GetNetworkId(),
        auth_op.account_id,
        auth_op.nonce,
        auth_op.payload);
    const auto signature = m_keystore.Sign(digest);
    if (!signature) {
        return {.error = SendMailError::CRYPTO_FAILURE, .error_message = "Failed to sign mail operation"};
    }
    auth_op.signature = *signature;

    ProtocolOperationV1 proto_op{auth_op};
    const uint256 mail_id = ComputeOperationId(proto_op);

    const auto submit_res = m_runtime.SubmitOperation(proto_op);
    if (!submit_res) {
        return {.error = SendMailError::SUBMIT_FAILED, .error_message = "Network rejected operation"};
    }

    MailItemV1 item;
    item.mail_id = mail_id;
    item.folder = MailFolder::SENT;
    item.sender = *sender_id;
    item.recipient = recipient;
    item.subject = subject;
    item.body = body;
    item.timestamp = mail.timestamp;
    item.read = true;
    item.finality = MailFinalityStatus::PENDING_FINALITY;
    item.salt = salt;
    item.content_commitment = content_commitment;
    item.discovery_tag = discovery_tag;
    item.fee = fee;

    m_messages.push_back(std::move(item));
    SaveMailbox();

    return {.error = SendMailError::NONE, .mail_id = mail_id, .fee = fee};
}

size_t CybouMailService::SyncMailbox()
{
    std::lock_guard lock(m_mutex);

    const auto my_account = m_keystore.GetAccountId();
    if (!my_account || my_account->IsNull()) {
        return 0;
    }

    const auto current_height_opt = m_runtime.GetFinalizedHeight();
    if (!current_height_opt) return 0;
    const uint64_t current_height = *current_height_opt;

    size_t new_inbox_count = 0;
    bool changed = false;

    for (uint64_t h = m_last_scanned_height + 1; h <= current_height; ++h) {
        const auto block_opt = m_runtime.GetBlockAtHeight(h);
        if (!block_opt) break;
        const auto& fin_block = *block_opt;
        const auto block_id = ComputeBlockId(fin_block.block);

        for (size_t op_idx = 0; op_idx < fin_block.block.operations.size(); ++op_idx) {
            const auto& proto_op = fin_block.block.operations[op_idx];
            if (!std::holds_alternative<AuthorizedOperationV1>(proto_op.payload)) {
                continue;
            }
            const auto& auth_op = std::get<AuthorizedOperationV1>(proto_op.payload);
            if (!std::holds_alternative<MailOpV1>(auth_op.payload)) {
                continue;
            }
            const auto& mail_op = std::get<MailOpV1>(auth_op.payload);
            const uint256 op_id = ComputeOperationId(proto_op);

            if (auth_op.account_id == *my_account) {
                for (auto& item : m_messages) {
                    if (item.folder == MailFolder::SENT &&
                        (item.mail_id == op_id || item.content_commitment == mail_op.content_commitment)) {
                        item.mail_id = op_id;
                        item.finality = MailFinalityStatus::FINAL;
                        item.block_height = h;
                        item.block_id = block_id;
                        item.operation_index = op_idx;

                        AccountAuthorizationV1 sender_auth{
                            .authorization_descriptor = auth_op.account_id.Value()
                        };
                        item.evidence_bundle = CreateMailEvidenceBundle(
                            fin_block.block,
                            op_idx,
                            fin_block.certificate,
                            sender_auth,
                            m_runtime.GetNetworkId());
                        changed = true;
                        break;
                    }
                }
            }

            if (mail_op.recipient == *my_account) {
                const bool already_present = std::any_of(
                    m_messages.begin(), m_messages.end(),
                    [&op_id](const MailItemV1& m) { return m.mail_id == op_id; });
                if (already_present) continue;

                const auto decrypted = DecryptMailPayload(
                    m_keystore,
                    auth_op.account_id,
                    mail_op.recipient,
                    mail_op.content_commitment,
                    mail_op.ciphertext);

                if (decrypted) {
                    MailItemV1 item;
                    item.mail_id = op_id;
                    item.folder = MailFolder::INBOX;
                    item.sender = decrypted->second.sender;
                    item.recipient = decrypted->second.recipient;
                    item.subject = decrypted->second.subject;
                    item.body = decrypted->second.body;
                    item.timestamp = decrypted->second.timestamp;
                    item.read = false;
                    item.finality = MailFinalityStatus::FINAL;
                    item.block_height = h;
                    item.block_id = block_id;
                    item.operation_index = op_idx;
                    item.salt = decrypted->first;
                    item.content_commitment = mail_op.content_commitment;
                    item.discovery_tag = mail_op.discovery_tag;
                    item.fee = MailFeeForSize(mail_op.ciphertext.size(), DevProtocolParameters());

                    AccountAuthorizationV1 sender_auth{
                        .authorization_descriptor = auth_op.account_id.Value()
                    };
                    item.evidence_bundle = CreateMailEvidenceBundle(
                        fin_block.block,
                        op_idx,
                        fin_block.certificate,
                        sender_auth,
                        m_runtime.GetNetworkId());

                    m_messages.push_back(std::move(item));
                    new_inbox_count++;
                    changed = true;
                }
            }
        }
        m_last_scanned_height = h;
        changed = true;
    }

    if (changed) {
        SaveMailbox();
    }
    return new_inbox_count;
}

uint64_t CybouMailService::GetLastScannedHeight() const
{
    std::lock_guard lock(m_mutex);
    return m_last_scanned_height;
}

} // namespace cybou
