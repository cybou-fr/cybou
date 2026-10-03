// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Вычисление BLAKE3-идентификаторов зашифрованных чанков.

#ifndef CYBOU_CHUNK_ID_H
#define CYBOU_CHUNK_ID_H

#include <array>
#include <span>

namespace cybou {

/// \brief 32-байтовый дайджест BLAKE3.
using Blake3Digest = std::array<unsigned char, 32>;
/// \brief Канонический идентификатор чанка: полный BLAKE3 exact stored bytes.
using ChunkId = Blake3Digest;

/// \brief Хэширует непрерывный буфер проверенной реализацией BLAKE3.
Blake3Digest ComputeBlake3Digest(std::span<const unsigned char> bytes);

/// \brief Хэширует несколько фрагментов как единый BLAKE3-поток без сборки общего буфера.
Blake3Digest ComputeBlake3Digest(std::span<const std::span<const unsigned char>> parts);

/// \brief Вычисляет ChunkId для точных сохраненных зашифрованных байтов.
ChunkId ComputeChunkId(std::span<const unsigned char> stored_encrypted_bytes);

} // namespace cybou

#endif // CYBOU_CHUNK_ID_H
