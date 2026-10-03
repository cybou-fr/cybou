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
    GRANT = 1,
    BURN = 2,
};

/// \brief Размер канонической сериализации PoaAuthAdjustment.
inline constexpr size_t POA_AUTH_ADJUSTMENT_SIZE{1 + 32 + 8 + 8 + 64 + 3309};

/// \brief PoA-подписанная операция изменения AUTH для одного аккаунта на заданной высоте блока.
struct PoaAuthAdjustment {
    PoaAuthAction action{PoaAuthAction::GRANT};
    AccountId target_account_id;
    uint64_t amount{0};
    uint64_t block_height{0};
    IdentityHybridSignature poa_signature;

    friend bool operator==(const PoaAuthAdjustment&, const PoaAuthAdjustment&) = default;
};

/// \brief Ошибки форматной, криптографической и stateful-проверки PoaAuthAdjustment.
enum class PoaAuthAdjustmentError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    WRONG_HEIGHT,
    INVALID_SIGNATURE,
    TARGET_NOT_FOUND,
    AUTHORITY_OVERFLOW,
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
