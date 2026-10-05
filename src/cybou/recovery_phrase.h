// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Кодирование и декодирование 24-словной recovery phrase.

#ifndef CYBOU_RECOVERY_PHRASE_H
#define CYBOU_RECOVERY_PHRASE_H

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace cybou {

using RecoveryEntropy = std::array<unsigned char, 32>;
using RecoveryWords = std::array<std::string, 24>;

/// Генерирует 256 бит recovery entropy для 24-словной фразы.
/// \return Новая entropy или `std::nullopt`, если CSPRNG недоступен.
/// \post Возвращаемая entropy является секретом и должна быть очищена владельцем копий.
/// \thread_safety Потокобезопасна.
std::optional<RecoveryEntropy> GenerateRecoveryEntropy();
/// Кодирует 256 бит entropy в каноническую 24-словную английскую recovery phrase.
/// \param entropy 256 бит recovery entropy.
/// \return Канонические 24 слова BIP-39 English.
/// \post Функция не валидирует внешний жизненный цикл секрета; слова нельзя логировать.
/// \thread_safety Потокобезопасна.
RecoveryWords EncodeRecoveryWords(const RecoveryEntropy& entropy);
/// Декодирует 24 слова обратно в 256 бит entropy и проверяет checksum BIP-39.
/// \param words Ровно 24 канонических слова.
/// \return Recovery entropy либо `std::nullopt`, если слово отсутствует в словаре или checksum неверна.
/// \post Возвращаемая entropy является секретом и не очищается автоматически.
/// \thread_safety Потокобезопасна.
std::optional<RecoveryEntropy> DecodeRecoveryWords(const RecoveryWords& words);

} // namespace cybou
#endif
