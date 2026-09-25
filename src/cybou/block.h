// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BLOCK_H
#define CYBOU_BLOCK_H

#include <cybou/bft.h>
#include <cybou/protocol_operation.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t CYBOU_BLOCK_VERSION{2};

/**
 * Canonical CYBOU block format.
 * Cryptographically binds parent, height, operations commitment, and post-execution state root.
 */
struct CybouBlock {
    uint8_t version{CYBOU_BLOCK_VERSION};
    uint256 parent_block_id;
    uint64_t height{0};
    std::vector<ProtocolOperation> operations;
    uint256 resulting_state_root;

    friend bool operator==(const CybouBlock&, const CybouBlock&) = default;
};

/**
 * Canonical block header extracting commitment roots without holding operations payload.
 */
struct CybouBlockHeader {
    uint8_t version{CYBOU_BLOCK_VERSION};
    uint256 parent_block_id;
    uint64_t height{0};
    uint256 operations_root;
    uint256 resulting_state_root;

    friend bool operator==(const CybouBlockHeader&, const CybouBlockHeader&) = default;
};

uint256 ComputeOperationsRootFromHashes(std::span<const uint256> hashes);
uint256 ComputeOperationsRoot(const std::vector<ProtocolOperation>& operations);
uint256 ComputeBlockHeaderId(const CybouBlockHeader& header);
uint256 ComputeBlockId(const CybouBlock& block);
CybouBlockHeader ExtractBlockHeader(const CybouBlock& block);

std::optional<std::vector<unsigned char>> SerializeBlock(const CybouBlock& block);
std::optional<CybouBlock> DeserializeBlock(std::span<const unsigned char> bytes);

/**
 * Finalized CYBOU block containing the canonical block and its BFT finality certificate.
 */
struct FinalizedBlock {
    CybouBlock block;
    BftFinalityCertificate certificate;

    friend bool operator==(const FinalizedBlock&, const FinalizedBlock&) = default;
};

std::optional<std::vector<unsigned char>> SerializeFinalizedBlock(const FinalizedBlock& finalized_block);
std::optional<FinalizedBlock> DeserializeFinalizedBlock(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_BLOCK_H
