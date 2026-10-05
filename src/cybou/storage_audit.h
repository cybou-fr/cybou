// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Взаимный аудит хранения (Mutual Proof of Storage) и верификация чанков (DEC-270).

#ifndef CYBOU_STORAGE_AUDIT_H
#define CYBOU_STORAGE_AUDIT_H

#include <cybou/account_id.h>
#include <cybou/chunk_id.h>
#include <cybou/hash256.h>
#include <cybou/identity_crypto.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Длина байтового среза чанка, участвующего в challenge-response.
inline constexpr std::size_t STORAGE_AUDIT_SAMPLE_SIZE{64};

/// \brief Челлендж взаимного аудита хранения (DEC-270).
struct StorageAuditChallenge {
    ChunkId chunk_id{};
    std::uint64_t byte_offset{0};
    std::array<unsigned char, 32> nonce{};

    friend bool operator==(const StorageAuditChallenge&, const StorageAuditChallenge&) = default;
};

/// \brief Аттестация доказательства физического владения чанком в ответ на челлендж (DEC-270).
struct StorageAuditProof {
    ChunkId chunk_id{};
    std::uint64_t byte_offset{0};
    std::array<unsigned char, 32> nonce{};
    cybou::Hash256 response_hash{};

    friend bool operator==(const StorageAuditProof&, const StorageAuditProof&) = default;
};

/// \brief Вычисляет детерминированный ответ на челлендж аудита хранения по телу чанка.
std::optional<cybou::Hash256> ComputeStorageAuditResponse(
    std::span<const unsigned char> chunk_bytes,
    std::uint64_t byte_offset,
    const std::array<unsigned char, 32>& nonce);

/// \brief Создает StorageAuditProof для заданного челленджа и содержимого чанка.
std::optional<StorageAuditProof> CreateStorageAuditProof(
    const StorageAuditChallenge& challenge,
    std::span<const unsigned char> chunk_bytes);

/// \brief Проверяет корректность StorageAuditProof против ожидаемых байтов чанка.
bool VerifyStorageAuditProof(
    const StorageAuditProof& proof,
    std::span<const unsigned char> chunk_bytes);

/// \brief Ответ provider на `StorageAuditChallenge` (DEC-276).
struct StorageAuditAnswer {
    /// \brief \c false: provider заявил, что admitted chunk у него отсутствует.
    bool held{false};
    /// \brief `ComputeStorageAuditResponse` по exact bytes; значим только при \p held.
    cybou::Hash256 response_hash{};
};

/// \brief Каноническое сообщение off-chain `StorageReceipt` (домен `CYBOU/STORAGE-RECEIPT`).
/// \details Provider подписывает его STORAGE-ключом после admission; receipt доказывает,
/// что держатель ключа принял exact chunk finalized публикации, но не будущую доступность.
std::vector<unsigned char> StorageReceiptMessage(const cybou::Hash256& network_binding,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id, std::uint32_t stored_size);

/// \brief Digest `StoragePayoutBinding`: домен `CYBOU/STORAGE-PAYOUT-BINDING`, сеть, StorageId, AccountID.
/// \details Его подписывают и STORAGE-ключ provider'а, и текущий Authorization-ключ payout-аккаунта (DEC-282).
std::array<unsigned char, 32> StoragePayoutBindingDigest(const cybou::Hash256& network_binding,
    const std::array<unsigned char, 32>& storage_id, const AccountId& payout_account);

/// \brief Привязка StorageId к payout-аккаунту, по которой placement выбирает экономические идентичности (DEC-280).
struct StoragePayoutBinding {
    AccountId payout_account;                 ///< Аккаунт, получающий выплаты за хранение.
    IdentityHybridSignature authorization;    ///< Подпись Authorization-ключа аккаунта над digest.
    std::vector<unsigned char> storage_proof; ///< STORAGE public key и подпись над тем же digest.

    friend bool operator==(const StoragePayoutBinding&, const StoragePayoutBinding&) = default;
};

/// \brief Кодирует binding: AccountID, Authorization-подпись (Ed25519 + ML-DSA-44) и storage proof.
std::vector<unsigned char> EncodeStoragePayoutBinding(const StoragePayoutBinding& binding);
/// \brief Декодирует binding с точным потреблением; подписи не проверяет.
std::optional<StoragePayoutBinding> DecodeStoragePayoutBinding(std::span<const unsigned char> bytes);
/// \brief Проверяет, что binding подписан STORAGE-ключом именно \p storage_id.
/// \details Authorization-подпись проверяет вызывающий против finalized Identity registry.
bool VerifyStoragePayoutBindingStorageKey(const StoragePayoutBinding& binding,
    const cybou::Hash256& network_binding, const std::array<unsigned char, 32>& storage_id);

/// \brief Проверяет подписанный receipt и возвращает StorageId подписавшего provider.
/// \param receipt Payload формата `STORAGE_PROOF`: STORAGE public key и hybrid-подпись.
/// \return StorageId либо \c std::nullopt при любой ошибке формата или подписи.
std::optional<std::array<unsigned char, 32>> VerifyStorageReceipt(std::span<const unsigned char> receipt,
    const cybou::Hash256& network_binding, const cybou::Hash256& publication_operation_id,
    const ChunkId& chunk_id, std::uint32_t stored_size);

} // namespace cybou

#endif // CYBOU_STORAGE_AUDIT_H
