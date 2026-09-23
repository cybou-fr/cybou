// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_EVIDENCE_H
#define CYBOU_EVIDENCE_H

#include <cybou/account_creation.h>
#include <cybou/bft.h>
#include <cybou/block.h>
#include <cybou/protocol_operation.h>
#include <cybou/validator.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t CYBOU_MAIL_EVIDENCE_VERSION{1};

/**
 * Proof of operation inclusion in a CybouBlockV1 operations list.
 * Stores operation index and all operation hashes in the block, which deterministically
 * reconstructs the operations_root committed in the block header.
 */
struct OperationInclusionProofV1 {
    uint32_t operation_index{0};
    std::vector<uint256> operation_hashes;

    friend bool operator==(const OperationInclusionProofV1&, const OperationInclusionProofV1&) = default;
};

/** Verify inclusion proof against an operation and the expected operations_root */
bool VerifyOperationInclusion(
    const OperationInclusionProofV1& proof,
    const ProtocolOperationV1& operation,
    const uint256& expected_operations_root);

/**
 * Self-contained cryptographic evidence bundle for a finalized MailTx.
 * Supports exporting and verifying:
 * - transaction inclusion in block
 * - BFT finality certificate
 * - historical sender-key authorization
 * - salted content commitment
 */
struct MailEvidenceBundleV1 {
    uint8_t version{CYBOU_MAIL_EVIDENCE_VERSION};
    uint256 network_id;
    AuthorizedOperationV1 mail_operation;
    CybouBlockHeaderV1 block_header;
    OperationInclusionProofV1 inclusion_proof;
    BftFinalityCertificateV1 finality_certificate;
    AccountAuthorizationV1 sender_authorization;

    friend bool operator==(const MailEvidenceBundleV1&, const MailEvidenceBundleV1&) = default;
};

enum class EvidenceVerificationError : uint8_t {
    NONE,
    UNSUPPORTED_VERSION,
    NETWORK_MISMATCH,
    NOT_A_MAIL_OPERATION,
    INVALID_OPERATION_SIGNATURE,
    INCLUSION_PROOF_FAILED,
    BLOCK_ID_MISMATCH,
    CERTIFICATE_HEIGHT_MISMATCH,
    CERTIFICATE_VERIFICATION_FAILED,
};

/** Verify a full MailEvidenceBundle against the network and active validator set */
EvidenceVerificationError VerifyMailEvidenceBundle(
    const MailEvidenceBundleV1& bundle,
    const ValidatorSetV1& validator_set,
    const uint256& expected_network_id);

/**
 * Verify voluntary plaintext and salt disclosure against the bundle's MailOpV1 content_commitment.
 * Returns true if SHA256("CYBOU/MAIL_CONTENT/V1" || salt || plaintext) == content_commitment.
 */
bool VerifyDisclosedMailContent(
    const MailEvidenceBundleV1& bundle,
    const uint256& salt,
    std::span<const unsigned char> plaintext);

/**
 * Helper to construct an evidence bundle from block, op index, finality cert, and sender authorization.
 */
std::optional<MailEvidenceBundleV1> CreateMailEvidenceBundle(
    const CybouBlockV1& block,
    size_t operation_index,
    BftFinalityCertificateV1 finality_certificate,
    AccountAuthorizationV1 sender_authorization,
    const uint256& network_id);

std::vector<unsigned char> SerializeMailEvidenceBundle(const MailEvidenceBundleV1& bundle);
std::optional<MailEvidenceBundleV1> DeserializeMailEvidenceBundle(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_EVIDENCE_H
