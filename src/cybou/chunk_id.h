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
///
/// Используется как базовый фиксированный контейнер для content-addressed
/// идентификаторов без дополнительного префикса длины.
using Blake3Digest = std::array<unsigned char, 32>;
/// \brief Канонический идентификатор чанка: полный BLAKE3 exact stored bytes.
///
/// Это именно хэш точных зашифрованных stored bytes, а не plaintext и не
/// метаданных публикации вокруг чанка.
using ChunkId = Blake3Digest;

/// \brief Хэширует непрерывный буфер проверенной реализацией BLAKE3.
/// \param bytes Непрерывный диапазон байтов для хэширования; пустой диапазон допустим.
/// \return Полный 32-байтовый BLAKE3 digest.
/// \pre Диапазон \p bytes должен оставаться валидным на всём протяжении вызова.
/// \post Входные байты не изменяются.
/// \par Потокобезопасность
/// Функция не хранит глобального состояния и потокобезопасна.
Blake3Digest ComputeBlake3Digest(std::span<const unsigned char> bytes);

/// \brief Хэширует несколько фрагментов как единый BLAKE3-поток без сборки общего буфера.
/// \param parts Последовательность фрагментов, которые логически конкатенируются в одном BLAKE3-потоке.
/// \return Полный 32-байтовый BLAKE3 digest конкатенации всех \p parts.
/// \pre Каждый span внутри \p parts должен оставаться валидным на всём протяжении вызова.
/// \post Входные байты не изменяются.
/// \par Потокобезопасность
/// Функция не хранит глобального состояния и потокобезопасна.
Blake3Digest ComputeBlake3Digest(std::span<const std::span<const unsigned char>> parts);

/// \brief Вычисляет ChunkId для точных сохраненных зашифрованных байтов.
/// \param stored_encrypted_bytes Exact stored bytes зашифрованного чанка.
/// \return Канонический ChunkId, равный полному BLAKE3 exact stored bytes.
/// \pre \p stored_encrypted_bytes должен содержать именно байты, которые будут храниться/передаваться как чанк.
/// \post Входные байты не изменяются.
/// \par Потокобезопасность
/// Функция не хранит глобального состояния и потокобезопасна.
ChunkId ComputeChunkId(std::span<const unsigned char> stored_encrypted_bytes);

} // namespace cybou

#endif // CYBOU_CHUNK_ID_H
