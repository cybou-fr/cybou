// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Канонический бинарный формат блока и финализированного блока CYBOU.

#ifndef CYBOU_BLOCK_H
#define CYBOU_BLOCK_H

#include <cybou/protocol_operation.h>
#include <cybou/poa_finality.h>
#include <cybou/hash256.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Верхняя граница размера блока, которую принимает локальный финализатор.
/// \details Локальный safety limit; превышение отвергается до подписи/коммита, чтобы один Full Node
/// не расходился с остальными из-за неограниченных аллокаций.
inline constexpr size_t MAX_FINALIZER_SERIALIZED_BLOCK_BYTES{32U * 1024U * 1024U};

/// \brief Канонический блок CYBOU, связывающий родителя, высоту, операции и итоговый state root.
struct CybouBlock {
    cybou::Hash256 parent_block_id;          ///< `BlockID` родителя; для высоты 1 ссылается на genesis anchor.
    uint64_t height{0};                      ///< Финализируемая высота блока.
    std::vector<ProtocolOperation> operations; ///< Упорядоченный список кандидат-операций, исполненный поверх родителя.
    cybou::Hash256 resulting_state_root;     ///< Итоговый `state root` после детерминированного исполнения `operations`.

    friend bool operator==(const CybouBlock&, const CybouBlock&) = default;
};

/// \brief Канонический заголовок блока без полезной нагрузки операций.
struct CybouBlockHeader {
    cybou::Hash256 parent_block_id;      ///< Родительский `BlockID`.
    uint64_t height{0};                  ///< Высота блока.
    cybou::Hash256 operations_root;      ///< Domain-separated коммитмент на точный порядок `OperationID`.
    cybou::Hash256 resulting_state_root; ///< `state root`, который обязаны воспроизвести все Full Node.

    friend bool operator==(const CybouBlockHeader&, const CybouBlockHeader&) = default;
};

/// \brief Коммитит упорядоченный список operation id/hash в один корень.
/// \param hashes Канонические 32-байтовые хэши операций в порядке блока.
/// \return Детерминированный `operations_root`.
cybou::Hash256 ComputeOperationsRootFromHashes(std::span<const cybou::Hash256> hashes);
/// \brief Сериализует и хэширует операции блока в канонический `operations_root`.
/// \param operations Операции в точном порядке включения.
/// \return Корень над каноническими сериализациями операций.
cybou::Hash256 ComputeOperationsRoot(const std::vector<ProtocolOperation>& operations);
/// \brief Вычисляет block id из канонического заголовка блока.
cybou::Hash256 ComputeBlockHeaderId(const CybouBlockHeader& header);
/// \brief Вычисляет block id из полного блока.
cybou::Hash256 ComputeBlockId(const CybouBlock& block);
/// \brief Извлекает заголовок блока с пересчётом operations root.
CybouBlockHeader ExtractBlockHeader(const CybouBlock& block);

/// \brief Сериализует блок в канонический бинарный формат.
std::optional<std::vector<unsigned char>> SerializeBlock(const CybouBlock& block);
/// \brief Десериализует блок и проверяет границы полезной нагрузки.
std::optional<CybouBlock> DeserializeBlock(std::span<const unsigned char> bytes);

/// \brief Финализированный блок: канонический блок плюс PoA-сертификат.
struct FinalizedBlock {
    CybouBlock block;
    PoaFinalityCertificate certificate;

    friend bool operator==(const FinalizedBlock&, const FinalizedBlock&) = default;
};

/// \brief Сериализует финализированный блок.
std::optional<std::vector<unsigned char>> SerializeFinalizedBlock(const FinalizedBlock& finalized_block);
/// \brief Десериализует финализированный блок и его PoA-сертификат.
std::optional<FinalizedBlock> DeserializeFinalizedBlock(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_BLOCK_H
