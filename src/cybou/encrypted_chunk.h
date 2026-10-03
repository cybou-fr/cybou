// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Формат одного зашифрованного чанка и операции encrypt/decrypt.

#ifndef CYBOU_ENCRYPTED_CHUNK_H
#define CYBOU_ENCRYPTED_CHUNK_H

#include <cybou/chunk_id.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief 32-байтовый ключ контента для одного неизменяемого encrypted tree.
using ContentKey = std::array<unsigned char, 32>;

inline constexpr std::size_t ENCRYPTED_CHUNK_HEADER_SIZE{48};
inline constexpr std::size_t ENCRYPTED_CHUNK_MIN_STORED_BYTES{ENCRYPTED_CHUNK_HEADER_SIZE + 1024 + 16};
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES{512 * 1024 - 4};
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_STORED_BYTES{
    ENCRYPTED_CHUNK_HEADER_SIZE + 512 * 1024 + 16};
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_RANDOM_PADDING_BYTES{256 * 1024};

/// \brief Один адресуемый зашифрованный чанк в stored-формате.
struct EncryptedChunk {
    ChunkId id{};
    std::vector<unsigned char> stored_bytes;
};

/// \brief Генерирует случайный content key для одного immutable encrypted tree.
std::optional<ContentKey> GenerateContentKey();

/// \brief Паддит и шифрует один ограниченный фрагмент plaintext.
std::optional<EncryptedChunk> EncryptChunk(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key,
    std::span<const unsigned char> plaintext);

/// \brief Проверяет ChunkId, аутентифицирует и возвращает plaintext одного чанка.
std::optional<std::vector<unsigned char>> DecryptChunk(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key,
    const ChunkId& expected_id,
    std::span<const unsigned char> stored_bytes);

} // namespace cybou

#endif // CYBOU_ENCRYPTED_CHUNK_H
