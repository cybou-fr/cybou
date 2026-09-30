// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_SERVICE_EVIDENCE_H
#define CYBOU_SERVICE_EVIDENCE_H
#include <cybou/node_binding.h>
#include <cybou/chunk_possession.h>
namespace cybou {
inline constexpr uint64_t AUTHORITY_STORAGE_BYTE_EPOCH_UNIT{1ULL << 30};
inline constexpr uint64_t FALSE_STORAGE_CLAIM_PENALTY{32};
inline constexpr uint64_t FALSE_VALIDATION_CLAIM_PENALTY{32};
inline constexpr uint32_t MAX_STORAGE_PLEDGES_PER_IDENTITY{4096};
enum class ServiceEvidenceKind : uint8_t { HEARTBEAT=1, STORAGE_COMMIT=2, STORAGE_RESPONSE=3, STORAGE_RELEASE=4, VALIDATION_RECEIPT=5 };
/** Signed dedicated-node evidence; no Identity credential or nonce is delegated to a node. */
struct ServiceEvidence {
    AccountId account_id;
    uint256 node_id;
    uint64_t base_height{0};
    uint256 base_block_id;
    ServiceEvidenceKind kind{ServiceEvidenceKind::HEARTBEAT};
    uint256 publication_id;
    ChunkId chunk_id{};
    std::optional<ChunkPossessionProof> possession;
    uint32_t authorization_index{0};
    std::vector<ChunkId> authorization_siblings;
    uint256 base_state_root;
    /** Exact subject bytes for a validation receipt; no nested service evidence. */
    std::vector<unsigned char> validated_operation;
    IdentityHybridSignature signature;
    friend bool operator==(const ServiceEvidence&, const ServiceEvidence&) = default;
};
struct StoragePledge {
    uint256 publication_id;
    uint256 node_id;
    uint64_t stored_bytes{0}, start_epoch{0}, epoch{0};
    uint32_t response_mask{0};
    bool false_claim{false};
    friend bool operator==(const StoragePledge&, const StoragePledge&) = default;
};
uint32_t ContributionSlots(uint64_t epoch_blocks);
std::optional<uint32_t> ContributionSlot(uint64_t height,uint64_t epoch_blocks);
uint32_t StorageChallengeLeaf(const uint256& network,const uint256& base,const AccountId& owner,const ChunkId& chunk,uint64_t size);
std::optional<std::vector<unsigned char>> SerializeServiceEvidence(const ServiceEvidence& evidence);
std::optional<ServiceEvidence> DeserializeServiceEvidence(std::span<const unsigned char> bytes);
std::optional<IdentityKeyId> ServiceEvidenceDigest(const uint256& network,const ServiceEvidence& evidence);
} // namespace cybou
#endif
