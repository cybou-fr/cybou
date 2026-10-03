// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief PoA-подписанная корректировка AUTH вне genesis и flat utility reward.

#ifndef CYBOU_POA_AUTH_ADJUSTMENT_H
#define CYBOU_POA_AUTH_ADJUSTMENT_H

#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Направление изменения AUTH, подписанного PoA finalizer'ом.
enum class PoaAuthAction : uint8_t {
    GRANT = 1, ///< Увеличить AUTH цели на `amount`.
    BURN = 2,  ///< Уменьшить AUTH цели на `min(current, amount)` без ухода в отрицательные значения.
};

/// \brief Размер канонической сериализации `PoaAuthAdjustment`.
/// \details `action` 1 + `target_account_id` 32 + `amount` 8 + `block_height` 8 +
/// PoA signature (Ed25519 64 + ML-DSA-65 3309).
inline constexpr size_t POA_AUTH_ADJUSTMENT_SIZE{1 + 32 + 8 + 8 + 64 + 3309};

/// \brief PoA-подписанная операция изменения AUTH для одного аккаунта на заданной высоте блока.
struct PoaAuthAdjustment {
    PoaAuthAction action{PoaAuthAction::GRANT}; ///< Направление изменения AUTH.
    AccountId target_account_id; ///< Аккаунт, чей `authority` меняется.
    uint64_t amount{0}; ///< Величина изменения AUTH; ноль запрещён.
    uint64_t block_height{0}; ///< Точная высота блока, в котором корректировка может быть финализирована.
    IdentityHybridSignature poa_signature; ///< PoA signature над domain-separated digest корректировки.

    friend bool operator==(const PoaAuthAdjustment&, const PoaAuthAdjustment&) = default;
};

/// \brief Ошибки форматной, криптографической и stateful-проверки PoaAuthAdjustment.
enum class PoaAuthAdjustmentError : uint8_t {
    NONE,               ///< Корректировка принята и применена.
    INVALID_PAYLOAD,    ///< Канонический payload недопустим.
    WRONG_HEIGHT,       ///< Корректировка привязана к другой высоте блока.
    INVALID_SIGNATURE,  ///< PoA signature недействительна или ключ имеет неверное назначение.
    TARGET_NOT_FOUND,   ///< Целевой аккаунт отсутствует.
    AUTHORITY_OVERFLOW, ///< `GRANT` переполняет `uint64_t`.
};

/// \brief Вычисляет domain-separated digest корректировки AUTH для подписи PoA.
std::optional<std::array<unsigned char, 32>> ComputePoaAuthAdjustmentDigest(
    const cybou::Hash256& network_binding, const PoaAuthAdjustment& adjustment);
/// \brief Сериализует корректировку AUTH в канонический бинарный формат.
std::optional<std::vector<unsigned char>> SerializePoaAuthAdjustment(const PoaAuthAdjustment& adjustment);
/// \brief Десериализует корректировку AUTH из канонического бинарного формата.
std::optional<PoaAuthAdjustment> DeserializePoaAuthAdjustment(std::span<const unsigned char> bytes);

/// \brief Применяет GRANT/BURN AUTH к кандидатному состоянию после проверки PoA-подписи и высоты.
PoaAuthAdjustmentError ApplyPoaAuthAdjustment(const PoaAuthAdjustment& adjustment,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const IdentityHybridPublicKey& poa_key, CybouState& state);

} // namespace cybou
#endif // CYBOU_POA_AUTH_ADJUSTMENT_H
