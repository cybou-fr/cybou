// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Минимальные Ed25519 helpers для пользовательских подписи и верификации.

#ifndef CYBOU_SIGNING_H
#define CYBOU_SIGNING_H

#include <cybou/hash256.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

/// \brief Размеры ключей, используемые низкоуровневыми signing helpers.
/// \details Ed25519 public key = 32 байта, ML-DSA-44 public key = 1312 байт,
/// ML-DSA-65 public key = 1952 байта согласно текущему каноническому криптографическому набору.
inline constexpr size_t ED25519_PUBLIC_KEY_SIZE{32};
inline constexpr size_t MLDSA44_PUBLIC_KEY_SIZE{1312};
inline constexpr size_t MLDSA65_PUBLIC_KEY_SIZE{1952};

/// \brief Размер Ed25519-подписи.
inline constexpr size_t USER_SIGNATURE_SIZE{64};

/// \brief Проверяет Ed25519-подпись над сообщением по 32-байтовому публичному ключу.
/// \return `false` при любом несоответствии длины, нулевом ключе или ошибке backend'а; функция fail-closed.
bool VerifyUserSignature(
    const cybou::Hash256& public_key,
    std::span<const unsigned char> signature,
    std::span<const unsigned char> message);

/// \brief Детерминированно выводит Ed25519-публичный ключ из 32-байтового seed.
/// \return Публичный ключ либо `std::nullopt`, если backend не смог импортировать seed.
std::optional<cybou::Hash256> DeriveEd25519PublicKey(std::span<const unsigned char, 32> private_key);

/// \brief Подписывает сообщение Ed25519 seed'ом и возвращает 64-байтовую подпись.
/// \return Подпись либо `std::nullopt`, если backend отказал.
/// \note Функция не хранит глобальное состояние и пригодна для детерминированного локального использования.
std::optional<std::array<unsigned char, USER_SIGNATURE_SIZE>> SignUserMessage(
    std::span<const unsigned char, 32> private_key,
    std::span<const unsigned char> message);

} // namespace cybou

#endif // CYBOU_SIGNING_H
