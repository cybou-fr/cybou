// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Реализация взаимного доказательства хранения (DEC-270).

#include <cybou/storage_audit.h>

#include <cybou/p2p/session.h>

#include <blake3.h>
#include <algorithm>
#include <string_view>

namespace cybou {

std::optional<cybou::Hash256> ComputeStorageAuditResponse(
    const std::span<const unsigned char> chunk_bytes,
    const std::uint64_t byte_offset,
    const std::array<unsigned char, 32>& nonce)
{
    if (chunk_bytes.empty() || byte_offset >= chunk_bytes.size()) {
        return std::nullopt;
    }

    const std::size_t available = chunk_bytes.size() - static_cast<std::size_t>(byte_offset);
    const std::size_t sample_len = std::min(available, STORAGE_AUDIT_SAMPLE_SIZE);
    const auto sample = chunk_bytes.subspan(static_cast<std::size_t>(byte_offset), sample_len);

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, sample.data(), sample.size());
    blake3_hasher_update(&hasher, nonce.data(), nonce.size());

    std::array<unsigned char, 32> out{};
    blake3_hasher_finalize(&hasher, out.data(), out.size());

    return cybou::Hash256{std::span<const unsigned char, 32>{out}};
}

std::optional<StorageAuditProof> CreateStorageAuditProof(
    const StorageAuditChallenge& challenge,
    const std::span<const unsigned char> chunk_bytes)
{
    if (ComputeChunkId(chunk_bytes) != challenge.chunk_id) {
        return std::nullopt;
    }
    const auto response = ComputeStorageAuditResponse(chunk_bytes, challenge.byte_offset, challenge.nonce);
    if (!response) {
        return std::nullopt;
    }
    return StorageAuditProof{
        .chunk_id = challenge.chunk_id,
        .byte_offset = challenge.byte_offset,
        .nonce = challenge.nonce,
        .response_hash = *response,
    };
}

bool VerifyStorageAuditProof(
    const StorageAuditProof& proof,
    const std::span<const unsigned char> chunk_bytes)
{
    if (ComputeChunkId(chunk_bytes) != proof.chunk_id) {
        return false;
    }
    const auto expected = ComputeStorageAuditResponse(chunk_bytes, proof.byte_offset, proof.nonce);
    return expected && *expected == proof.response_hash;
}

std::vector<unsigned char> StorageReceiptMessage(const cybou::Hash256& network_binding,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id, const std::uint32_t stored_size)
{
    constexpr std::string_view DOMAIN{"CYBOU/STORAGE-RECEIPT"};
    std::vector<unsigned char> message(DOMAIN.begin(), DOMAIN.end());
    message.insert(message.end(), network_binding.begin(), network_binding.end());
    message.insert(message.end(), publication_operation_id.begin(), publication_operation_id.end());
    message.insert(message.end(), chunk_id.begin(), chunk_id.end());
    for (int i = 0; i < 4; ++i) message.push_back(static_cast<unsigned char>(stored_size >> (8 * i)));
    return message;
}

std::optional<std::array<unsigned char, 32>> VerifyStorageReceipt(const std::span<const unsigned char> receipt,
    const cybou::Hash256& network_binding, const cybou::Hash256& publication_operation_id,
    const ChunkId& chunk_id, const std::uint32_t stored_size)
{
    // Receipt использует тот же формат key+signature, что и `STORAGE_PROOF`, но другой домен сообщения.
    return p2p::VerifyStorageProof(receipt,
        StorageReceiptMessage(network_binding, publication_operation_id, chunk_id, stored_size));
}

} // namespace cybou
