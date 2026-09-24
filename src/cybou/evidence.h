// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_EVIDENCE_H
#define CYBOU_EVIDENCE_H

#include <cybou/account_creation.h>
#include <cybou/bft.h>
#include <cybou/block.h>
#include <cybou/mail_tx.h>
#include <cybou/protocol_operation.h>
#include <cybou/validator.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t CYBOU_MAIL_EVIDENCE_VERSION{2};

/**
 * Proof of operation inclusion in a CybouBlock operations list.
 * Stores operation index and all operation hashes in the block, which deterministically
 * reconstructs the operations_root committed in the block header.
 */
struct OperationInclusionProof {
    uint32_t operation_index{0};
    std::vector<uint256> operation_hashes;

    friend bool operator==(const OperationInclusionProof&, const OperationInclusionProof&) = default;
};

/** Verify inclusion proof against an operation and the expected operations_root */
bool VerifyOperationInclusion(
    const OperationInclusionProof& proof,
    const ProtocolOperation& operation,
    const uint256& expected_operations_root);

/**
 * Evidence bundle for a finalized MailTx.
 * Supports exporting and verifying:
 * - transaction inclusion in block
 * - BFT finality certificate
 * - signature against the supplied sender device key
 * - salted content commitment
 */
struct MailEvidenceBundle {
    uint8_t version{CYBOU_MAIL_EVIDENCE_VERSION};
    uint256 network_id;
    AuthorizedMail mail_operation;
    CybouBlockHeader block_header;
    OperationInclusionProof inclusion_proof;
    BftFinalityCertificate finality_certificate;
    IdentityHybridPublicKey sender_device_key;

    friend bool operator==(const MailEvidenceBundle&, const MailEvidenceBundle&) = default;
};

enum class EvidenceVerificationError : uint8_t {
    NONE,
    UNSUPPORTED_VERSION,
    NETWORK_MISMATCH,
    INVALID_OPERATION_SIGNATURE,
    INCLUSION_PROOF_FAILED,
    BLOCK_ID_MISMATCH,
    CERTIFICATE_HEIGHT_MISMATCH,
    CERTIFICATE_VERIFICATION_FAILED,
};

/** Verify a full MailEvidenceBundle against the network and active validator set */
EvidenceVerificationError VerifyMailEvidenceBundle(
    const MailEvidenceBundle& bundle,
    const ValidatorSet& validator_set,
    const uint256& expected_network_id);

/**
 * Verify voluntary plaintext and discovery tag disclosure against the bundle's content_commitment.
 */
bool VerifyDisclosedMailContent(
    const MailEvidenceBundle& bundle,
    const uint256& content_commitment);

std::optional<MailEvidenceBundle> CreateMailEvidenceBundle(
    const CybouBlock& block,
    size_t operation_index,
    BftFinalityCertificate finality_certificate,
    IdentityHybridPublicKey sender_device_key,
    const uint256& network_id);

std::optional<std::vector<unsigned char>> SerializeMailEvidenceBundle(const MailEvidenceBundle& bundle);
std::optional<MailEvidenceBundle> DeserializeMailEvidenceBundle(std::span<const unsigned char> bytes);

// Transition aliases
using OperationInclusionProofV1 = OperationInclusionProof;
using MailEvidenceBundleV1 = MailEvidenceBundle;

} // namespace cybou

#endif // CYBOU_EVIDENCE_H
