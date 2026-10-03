// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
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
inline constexpr size_t MAX_FINALIZER_SERIALIZED_BLOCK_BYTES{32U * 1024U * 1024U};

/// \brief Канонический блок CYBOU, связывающий родителя, высоту, операции и итоговый state root.
struct CybouBlock {
    cybou::Hash256 parent_block_id;
    uint64_t height{0};
    std::vector<ProtocolOperation> operations;
    cybou::Hash256 resulting_state_root;

    friend bool operator==(const CybouBlock&, const CybouBlock&) = default;
};

/// \brief Канонический заголовок блока без полезной нагрузки операций.
struct CybouBlockHeader {
    cybou::Hash256 parent_block_id;
    uint64_t height{0};
    cybou::Hash256 operations_root;
    cybou::Hash256 resulting_state_root;

    friend bool operator==(const CybouBlockHeader&, const CybouBlockHeader&) = default;
};

/// \brief Коммитит упорядоченный список operation id/hash в один корень.
cybou::Hash256 ComputeOperationsRootFromHashes(std::span<const cybou::Hash256> hashes);
/// \brief Сериализует и хэширует операции блока в канонический operations root.
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
