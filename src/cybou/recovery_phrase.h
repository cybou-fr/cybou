// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

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
std::optional<RecoveryEntropy> GenerateRecoveryEntropy();
/// Кодирует 256 бит entropy в каноническую 24-словную английскую recovery phrase.
RecoveryWords EncodeRecoveryWords(const RecoveryEntropy& entropy);
/// Декодирует 24 слова обратно в 256 бит entropy и проверяет checksum BIP-39.
std::optional<RecoveryEntropy> DecodeRecoveryWords(const RecoveryWords& words);

} // namespace cybou
#endif
