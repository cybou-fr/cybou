// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_RECOVERY_PHRASE_H
#define CYBOU_RECOVERY_PHRASE_H

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace cybou {

using RecoveryEntropy = std::array<unsigned char, 32>;
using RecoveryWords = std::array<std::string, 24>;

// BIP-39 English ENT=256 encoding. CYBOU derives its keys directly from the
// recovered entropy; it does not use BIP-39's PBKDF2 wallet seed or passphrase.
std::optional<RecoveryEntropy> GenerateRecoveryEntropy();
RecoveryWords EncodeRecoveryWords(const RecoveryEntropy& entropy);
std::optional<RecoveryEntropy> DecodeRecoveryWords(const RecoveryWords& words);

} // namespace cybou
#endif
