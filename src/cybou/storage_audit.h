// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Взаимный аудит хранения (Mutual Proof of Storage) и верификация чанков (DEC-270).

#ifndef CYBOU_STORAGE_AUDIT_H
#define CYBOU_STORAGE_AUDIT_H

#include <cybou/chunk_id.h>
#include <cybou/hash256.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>

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

} // namespace cybou

#endif // CYBOU_STORAGE_AUDIT_H
