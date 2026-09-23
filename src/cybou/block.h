// Copyright (c) 2026 Stanislav Saveliev
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

inline constexpr uint8_t CYBOU_BLOCK_VERSION{1};

/**
 * Canonical CYBOU block format.
 * Cryptographically binds parent, height, operations commitment, and post-execution state root.
 */
struct CybouBlockV1 {
    uint8_t version{CYBOU_BLOCK_VERSION};
    uint256 parent_block_id;
    uint64_t height{0};
    std::vector<ProtocolOperationV1> operations;
    uint256 resulting_state_root;

    friend bool operator==(const CybouBlockV1&, const CybouBlockV1&) = default;
};

/** Compute domain-separated operations commitment root: SHA256("CYBOU/OPS_ROOT/V1" || count || hashes) */
uint256 ComputeOperationsRoot(const std::vector<ProtocolOperationV1>& operations);

/** Compute domain-separated BlockID: SHA256("CYBOU/BLOCK/V1" || version || parent || height || ops_root || state_root) */
uint256 ComputeBlockId(const CybouBlockV1& block);

std::vector<unsigned char> SerializeBlock(const CybouBlockV1& block);
std::optional<CybouBlockV1> DeserializeBlock(std::span<const unsigned char> bytes);

/**
 * Finalized CYBOU block containing the canonical block and its BFT 2/3+ finality certificate.
 */
struct FinalizedBlockV1 {
    CybouBlockV1 block;
    BftFinalityCertificateV1 certificate;

    friend bool operator==(const FinalizedBlockV1&, const FinalizedBlockV1&) = default;
};

std::vector<unsigned char> SerializeFinalizedBlock(const FinalizedBlockV1& finalized_block);
std::optional<FinalizedBlockV1> DeserializeFinalizedBlock(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_BLOCK_H
