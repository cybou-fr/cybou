// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief PoA finality certificate и его каноническая сериализация.

#ifndef CYBOU_POA_FINALITY_H
#define CYBOU_POA_FINALITY_H

#include <cybou/identity_crypto.h>
#include <cybou/hash256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

struct CybouBlock;

/// \brief Размер канонической сериализации `PoaFinalityCertificate`.
/// \details `network_binding` 32 + `block_id` 32 + `height` 8 + `parent_block_id` 32 +
/// Ed25519 signature 64 + ML-DSA-65 signature 3309.
inline constexpr size_t POA_FINALITY_CERTIFICATE_SIZE{ 32 + 32 + 8 + 32 + 64 + 3309};

/// \brief Гибридный PoA-сертификат, финализирующий ровно один канонический блок.
struct PoaFinalityCertificate {
    cybou::Hash256 network_binding; ///< Точная сеть, в которой сертификат действителен.
    cybou::Hash256 block_id; ///< `BlockID` единственного финализируемого блока.
    uint64_t height{0}; ///< Высота финализируемого блока; ноль запрещён.
    cybou::Hash256 parent_block_id; ///< Родитель выбранного блока; часть anti-equivocation binding.
    IdentityHybridSignature signature; ///< PoA signature (Ed25519 + ML-DSA-65) над finality digest.

    friend bool operator==(const PoaFinalityCertificate&, const PoaFinalityCertificate&) = default;
};

/// \brief Вычисляет domain-separated digest, который подписывает PoA finalizer.
/// \param network_binding Привязка сети.
/// \param block_id Идентификатор блока.
/// \param height Высота блока.
/// \param parent_block_id Идентификатор родителя.
/// \return Детерминированный digest `CYBOU/POA-FINALITY`.
cybou::Hash256 ComputePoaFinalityDigest(const cybou::Hash256& network_binding, const cybou::Hash256& block_id,
    uint64_t height, const cybou::Hash256& parent_block_id);

/// \brief Проверяет PoA-сертификат на точное совпадение сети, блока, высоты и родителя.
/// \return `false` при любом несоответствии полей, формата или подписи; функция fail-closed.
bool VerifyPoaFinalityCertificate(const PoaFinalityCertificate& certificate,
    const IdentityHybridPublicKey& genesis_finalizer_key,
    const cybou::Hash256& expected_network_binding, const cybou::Hash256& expected_block_id,
    uint64_t expected_height, const cybou::Hash256& expected_parent_block_id);

/// \brief Проверяет сертификат против полного канонического блока, который он финализирует.
bool VerifyPoaCertificateForBlock(const PoaFinalityCertificate& certificate,
    const IdentityHybridPublicKey& genesis_finalizer_key,
    const cybou::Hash256& expected_network_binding, const CybouBlock& block);

/// \brief Сериализует PoA-сертификат в канонический бинарный формат.
std::optional<std::vector<unsigned char>> SerializePoaFinalityCertificate(
    const PoaFinalityCertificate& certificate);
/// \brief Десериализует PoA-сертификат из канонического бинарного формата.
std::optional<PoaFinalityCertificate> DeserializePoaFinalityCertificate(
    std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_POA_FINALITY_H
