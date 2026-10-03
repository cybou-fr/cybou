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

/// \brief Размер незашифрованного заголовка stored-формата.
///
/// Разложение: 4 байта magic `"CYCH"` + 32 байта HKDF salt + 12 байт
/// ChaCha20-Poly1305 nonce = 48 байт.
inline constexpr std::size_t ENCRYPTED_CHUNK_HEADER_SIZE{48};
/// \brief Минимальный допустимый размер stored-чанка.
///
/// Разложение: заголовок 48 + минимальный pad bucket 1024 + тег AEAD 16.
inline constexpr std::size_t ENCRYPTED_CHUNK_MIN_STORED_BYTES{ENCRYPTED_CHUNK_HEADER_SIZE + 1024 + 16};
/// \brief Максимальный размер полезного plaintext в одном чанке.
///
/// Разложение: верхний bucket 512 KiB минус 4 байта длины plaintext внутри
/// зашифрованного frame.
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES{512 * 1024 - 4};
/// \brief Максимальный допустимый размер stored-чанка.
///
/// Разложение: заголовок 48 + максимальный зашифрованный frame 512 KiB +
/// тег AEAD 16.
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_STORED_BYTES{
    ENCRYPTED_CHUNK_HEADER_SIZE + 512 * 1024 + 16};
/// \brief Верхняя граница случайного дополнительного padding сверх минимального bucket.
///
/// Значение 256 KiB ограничивает скрытие точного размера без неограниченного
/// раздувания ciphertext.
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_RANDOM_PADDING_BYTES{256 * 1024};

/// \brief Один адресуемый зашифрованный чанк в stored-формате.
struct EncryptedChunk {
    /// \brief ChunkId, равный BLAKE3 exact stored bytes поля \ref stored_bytes.
    ChunkId id{};
    /// \brief Exact stored bytes, пригодные для локального хранения и P2P-передачи.
    std::vector<unsigned char> stored_bytes;
};

/// \brief Генерирует случайный content key для одного immutable encrypted tree.
/// \return 32-байтовый ContentKey либо \c std::nullopt при отказе RNG или при
/// генерации недопустимого нулевого ключа.
/// \post При успехе возвращённый ключ не равен нулевому массиву.
/// \par Потокобезопасность
/// Не использует разделяемое состояние этого модуля; потокобезопасность RNG
/// определяется используемой криптобиблиотекой.
std::optional<ContentKey> GenerateContentKey();

/// \brief Паддит и шифрует один ограниченный фрагмент plaintext.
/// \param network_binding 32-байтовый NetworkBinding, включаемый в HKDF и AAD.
/// \param content_key 32-байтовый ContentKey дерева, из которого выводится ключ чанка.
/// \param plaintext Полезные байты одного DATA/INDEX/ROOT payload.
/// \return Сформированный EncryptedChunk либо \c std::nullopt при нарушении
/// лимитов, отказе RNG или неудаче криптографических операций.
/// \pre \p plaintext.size() не должен превышать \ref ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES.
/// \post При успехе `result->id == ComputeChunkId(result->stored_bytes)`.
/// \post Входные диапазоны не изменяются.
/// \par Потокобезопасность
/// Не использует разделяемое состояние этого модуля; потокобезопасность RNG и
/// криптографии определяется используемыми библиотеками.
std::optional<EncryptedChunk> EncryptChunk(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key,
    std::span<const unsigned char> plaintext);

/// \brief Проверяет ChunkId, аутентифицирует и возвращает plaintext одного чанка.
/// \param network_binding 32-байтовый NetworkBinding, который должен совпадать с использованным при EncryptChunk().
/// \param content_key 32-байтовый ContentKey дерева, использованный для вывода ключа чанка.
/// \param expected_id Ожидаемый ChunkId exact stored bytes.
/// \param stored_bytes Exact stored bytes принятого чанка.
/// \return Расшифрованный plaintext либо \c std::nullopt при несовпадении
/// ChunkId, ошибке формата, ошибке аутентификации или нарушении padding-лимитов.
/// \pre \p stored_bytes должен содержать exact stored bytes одного чанка в текущем формате.
/// \post При успехе размер результата не превышает \ref ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES.
/// \post Входные диапазоны не изменяются.
/// \par Потокобезопасность
/// Не использует разделяемое состояние этого модуля; потокобезопасность
/// криптографии определяется используемыми библиотеками.
std::optional<std::vector<unsigned char>> DecryptChunk(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key,
    const ChunkId& expected_id,
    std::span<const unsigned char> stored_bytes);

} // namespace cybou

#endif // CYBOU_ENCRYPTED_CHUNK_H
