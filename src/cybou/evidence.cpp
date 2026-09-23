// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/evidence.h>

#include <crypto/sha256.h>

#include <algorithm>

namespace cybou {

bool VerifyOperationInclusion(
    const OperationInclusionProofV1& proof,
    const ProtocolOperationV1& operation,
    const uint256& expected_operations_root)
{
    if (proof.operation_hashes.empty() || proof.operation_index >= proof.operation_hashes.size()) {
        return false;
    }

    const auto serialized_op = SerializeProtocolOperation(operation);
    uint256 op_hash;
    CSHA256().Write(serialized_op.data(), serialized_op.size()).Finalize(op_hash.begin());

    if (proof.operation_hashes[proof.operation_index] != op_hash) {
        return false;
    }

    const uint256 computed_root = ComputeOperationsRootFromHashes(proof.operation_hashes);
    return computed_root == expected_operations_root;
}

EvidenceVerificationError VerifyMailEvidenceBundle(
    const MailEvidenceBundleV1& bundle,
    const ValidatorSetV1& validator_set,
    const uint256& expected_network_id)
{
    if (bundle.version != CYBOU_MAIL_EVIDENCE_VERSION) {
        return EvidenceVerificationError::UNSUPPORTED_VERSION;
    }

    if (bundle.network_id != expected_network_id) {
        return EvidenceVerificationError::NETWORK_MISMATCH;
    }

    if (!std::holds_alternative<MailOpV1>(bundle.mail_operation.payload)) {
        return EvidenceVerificationError::NOT_A_MAIL_OPERATION;
    }

    // Verify sender signature over user operation digest using historical sender authorization key
    const uint256 op_digest = ComputeUserOperationDigest(
        bundle.network_id,
        bundle.mail_operation.account_id,
        bundle.mail_operation.nonce,
        bundle.mail_operation.payload);

    if (!VerifyUserSignature(
            bundle.sender_authorization.authorization_descriptor,
            bundle.mail_operation.signature,
            std::span<const unsigned char>{op_digest.begin(), op_digest.size()})) {
        return EvidenceVerificationError::INVALID_OPERATION_SIGNATURE;
    }

    // Verify transaction inclusion in block
    const ProtocolOperationV1 proto_op{bundle.mail_operation};
    if (!VerifyOperationInclusion(bundle.inclusion_proof, proto_op, bundle.block_header.operations_root)) {
        return EvidenceVerificationError::INCLUSION_PROOF_FAILED;
    }

    // Verify block header matches certificate
    const uint256 block_id = ComputeBlockHeaderId(bundle.block_header);
    if (block_id != bundle.finality_certificate.block_id) {
        return EvidenceVerificationError::BLOCK_ID_MISMATCH;
    }

    if (bundle.block_header.height != bundle.finality_certificate.height) {
        return EvidenceVerificationError::CERTIFICATE_HEIGHT_MISMATCH;
    }

    // Verify BFT finality certificate over the validator set
    if (VerifyFinalityCertificate(bundle.finality_certificate, validator_set, bundle.network_id) !=
        FinalityVerificationError::NONE) {
        return EvidenceVerificationError::CERTIFICATE_VERIFICATION_FAILED;
    }

    return EvidenceVerificationError::NONE;
}

bool VerifyDisclosedMailContent(
    const MailEvidenceBundleV1& bundle,
    const uint256& salt,
    std::span<const unsigned char> plaintext)
{
    if (!std::holds_alternative<MailOpV1>(bundle.mail_operation.payload)) {
        return false;
    }
    const auto& mail_op = std::get<MailOpV1>(bundle.mail_operation.payload);
    const uint256 computed_commitment = ComputeMailContentCommitment(salt, plaintext);
    return computed_commitment == mail_op.content_commitment;
}

std::optional<MailEvidenceBundleV1> CreateMailEvidenceBundle(
    const CybouBlockV1& block,
    size_t operation_index,
    BftFinalityCertificateV1 finality_certificate,
    AccountAuthorizationV1 sender_authorization,
    const uint256& network_id)
{
    if (operation_index >= block.operations.size()) {
        return std::nullopt;
    }

    const auto& op = block.operations[operation_index];
    if (!std::holds_alternative<AuthorizedOperationV1>(op.payload)) {
        return std::nullopt;
    }

    const auto& auth_op = std::get<AuthorizedOperationV1>(op.payload);
    if (!std::holds_alternative<MailOpV1>(auth_op.payload)) {
        return std::nullopt;
    }

    OperationInclusionProofV1 proof;
    proof.operation_index = static_cast<uint32_t>(operation_index);
    proof.operation_hashes.reserve(block.operations.size());
    for (const auto& item : block.operations) {
        const auto serialized = SerializeProtocolOperation(item);
        uint256 h;
        CSHA256().Write(serialized.data(), serialized.size()).Finalize(h.begin());
        proof.operation_hashes.push_back(h);
    }

    CybouBlockHeaderV1 header = ExtractBlockHeader(block);

    return MailEvidenceBundleV1{
        .version = CYBOU_MAIL_EVIDENCE_VERSION,
        .network_id = network_id,
        .mail_operation = auth_op,
        .block_header = std::move(header),
        .inclusion_proof = std::move(proof),
        .finality_certificate = std::move(finality_certificate),
        .sender_authorization = std::move(sender_authorization),
    };
}

std::vector<unsigned char> SerializeMailEvidenceBundle(const MailEvidenceBundleV1& bundle)
{
    std::vector<unsigned char> out;
    out.push_back(bundle.version);
    out.insert(out.end(), bundle.network_id.begin(), bundle.network_id.end());

    // mail_operation
    const auto serialized_op = SerializeAuthorizedOperation(bundle.mail_operation);
    const uint32_t op_len = static_cast<uint32_t>(serialized_op.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(op_len >> (8 * i)));
    out.insert(out.end(), serialized_op.begin(), serialized_op.end());

    // block_header: version (1), parent (32), height (8), ops_root (32), state_root (32)
    out.push_back(bundle.block_header.version);
    out.insert(out.end(), bundle.block_header.parent_block_id.begin(), bundle.block_header.parent_block_id.end());
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(bundle.block_header.height >> (8 * i)));
    out.insert(out.end(), bundle.block_header.operations_root.begin(), bundle.block_header.operations_root.end());
    out.insert(out.end(), bundle.block_header.resulting_state_root.begin(), bundle.block_header.resulting_state_root.end());

    // inclusion_proof: operation_index (4), count (4), hashes (count * 32)
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(bundle.inclusion_proof.operation_index >> (8 * i)));
    const uint32_t hash_count = static_cast<uint32_t>(bundle.inclusion_proof.operation_hashes.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(hash_count >> (8 * i)));
    for (const auto& h : bundle.inclusion_proof.operation_hashes) {
        out.insert(out.end(), h.begin(), h.end());
    }

    // finality_certificate
    const auto serialized_cert = SerializeFinalityCertificate(bundle.finality_certificate);
    const uint32_t cert_len = static_cast<uint32_t>(serialized_cert.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(cert_len >> (8 * i)));
    out.insert(out.end(), serialized_cert.begin(), serialized_cert.end());

    // sender_authorization
    const auto serialized_auth = SerializeAccountAuthorization(bundle.sender_authorization);
    const uint32_t auth_len = static_cast<uint32_t>(serialized_auth.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(auth_len >> (8 * i)));
    out.insert(out.end(), serialized_auth.begin(), serialized_auth.end());

    return out;
}

std::optional<MailEvidenceBundleV1> DeserializeMailEvidenceBundle(std::span<const unsigned char> bytes)
{
    // Minimal header: version (1) + network_id (32) + op_len (4)
    if (bytes.size() < 1 + 32 + 4) return std::nullopt;
    if (bytes[0] != CYBOU_MAIL_EVIDENCE_VERSION) return std::nullopt;

    uint256 network_id;
    std::copy_n(bytes.begin() + 1, 32, network_id.begin());

    size_t offset = 1 + 32;

    // mail_operation
    uint32_t op_len{0};
    for (int i = 0; i < 4; ++i) op_len |= uint32_t{bytes[offset + i]} << (8 * i);
    offset += 4;
    if (bytes.size() < offset + op_len) return std::nullopt;

    auto auth_op = DeserializeAuthorizedOperation(bytes.subspan(offset, op_len));
    if (!auth_op.has_value()) return std::nullopt;
    offset += op_len;

    // block_header: version (1) + parent (32) + height (8) + ops_root (32) + state_root (32) = 105 bytes
    static constexpr size_t HEADER_SIZE{1 + 32 + 8 + 32 + 32};
    if (bytes.size() < offset + HEADER_SIZE) return std::nullopt;

    CybouBlockHeaderV1 header;
    header.version = bytes[offset++];
    std::copy_n(bytes.begin() + offset, 32, header.parent_block_id.begin());
    offset += 32;

    uint64_t height{0};
    for (int i = 0; i < 8; ++i) height |= uint64_t{bytes[offset + i]} << (8 * i);
    header.height = height;
    offset += 8;

    std::copy_n(bytes.begin() + offset, 32, header.operations_root.begin());
    offset += 32;

    std::copy_n(bytes.begin() + offset, 32, header.resulting_state_root.begin());
    offset += 32;

    // inclusion_proof: operation_index (4) + count (4) + count * 32
    if (bytes.size() < offset + 8) return std::nullopt;
    uint32_t op_index{0};
    for (int i = 0; i < 4; ++i) op_index |= uint32_t{bytes[offset + i]} << (8 * i);
    offset += 4;

    uint32_t hash_count{0};
    for (int i = 0; i < 4; ++i) hash_count |= uint32_t{bytes[offset + i]} << (8 * i);
    offset += 4;

    if (bytes.size() < offset + static_cast<size_t>(hash_count) * 32) return std::nullopt;
    OperationInclusionProofV1 proof;
    proof.operation_index = op_index;
    proof.operation_hashes.reserve(hash_count);
    for (uint32_t i = 0; i < hash_count; ++i) {
        uint256 h;
        std::copy_n(bytes.begin() + offset, 32, h.begin());
        offset += 32;
        proof.operation_hashes.push_back(h);
    }

    // finality_certificate
    if (bytes.size() < offset + 4) return std::nullopt;
    uint32_t cert_len{0};
    for (int i = 0; i < 4; ++i) cert_len |= uint32_t{bytes[offset + i]} << (8 * i);
    offset += 4;

    if (bytes.size() < offset + cert_len) return std::nullopt;
    auto cert = DeserializeFinalityCertificate(bytes.subspan(offset, cert_len));
    if (!cert.has_value()) return std::nullopt;
    offset += cert_len;

    // sender_authorization
    if (bytes.size() < offset + 4) return std::nullopt;
    uint32_t auth_len{0};
    for (int i = 0; i < 4; ++i) auth_len |= uint32_t{bytes[offset + i]} << (8 * i);
    offset += 4;

    if (bytes.size() != offset + auth_len) return std::nullopt;
    auto auth = DeserializeAccountAuthorization(bytes.subspan(offset, auth_len));
    if (!auth.has_value()) return std::nullopt;

    return MailEvidenceBundleV1{
        .version = bytes[0],
        .network_id = network_id,
        .mail_operation = std::move(*auth_op),
        .block_header = std::move(header),
        .inclusion_proof = std::move(proof),
        .finality_certificate = std::move(*cert),
        .sender_authorization = std::move(*auth),
    };
}

} // namespace cybou
