// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/evidence.h>

#include <crypto/sha256.h>

#include <algorithm>

namespace cybou {

namespace {

inline void AppendUint64LE(std::vector<unsigned char>& out, uint64_t val)
{
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline void AppendUint32LE(std::vector<unsigned char>& out, uint32_t val)
{
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline uint64_t ReadUint64LE(const std::span<const unsigned char>& bytes, size_t offset)
{
    uint64_t val{0};
    for (int i = 0; i < 8; ++i) {
        val |= uint64_t{bytes[offset + i]} << (8 * i);
    }
    return val;
}

inline uint32_t ReadUint32LE(const std::span<const unsigned char>& bytes, size_t offset)
{
    uint32_t val{0};
    for (int i = 0; i < 4; ++i) {
        val |= uint32_t{bytes[offset + i]} << (8 * i);
    }
    return val;
}

} // namespace

bool VerifyOperationInclusion(
    const OperationInclusionProof& proof,
    const ProtocolOperation& operation,
    const uint256& expected_operations_root)
{
    if (proof.operation_hashes.empty() || proof.operation_index >= proof.operation_hashes.size()) {
        return false;
    }

    const auto serialized_op = SerializeProtocolOperation(operation);
    if (!serialized_op) return false;
    uint256 op_hash;
    CSHA256().Write(serialized_op->data(), serialized_op->size()).Finalize(op_hash.begin());

    if (proof.operation_hashes[proof.operation_index] != op_hash) {
        return false;
    }

    const uint256 computed_root = ComputeOperationsRootFromHashes(proof.operation_hashes);
    return computed_root == expected_operations_root;
}

EvidenceVerificationError VerifyMailEvidenceBundle(
    const MailEvidenceBundle& bundle,
    const ValidatorSet& validator_set,
    const uint256& expected_network_id)
{
    if (bundle.version != CYBOU_MAIL_EVIDENCE_VERSION) {
        return EvidenceVerificationError::UNSUPPORTED_VERSION;
    }

    if (bundle.network_id != expected_network_id) {
        return EvidenceVerificationError::NETWORK_MISMATCH;
    }

    // Verify sender device public key matches the device ID in the mail authorization
    const auto sender_key_id = ComputeDeviceKeyId(bundle.sender_device_key);
    if (!sender_key_id || *sender_key_id != bundle.mail_operation.authorization.device_id) {
        return EvidenceVerificationError::INVALID_OPERATION_SIGNATURE;
    }

    // Verify payload commitment matches
    const auto commitment = ComputeMailPayloadCommitment(bundle.mail_operation.mail);
    if (!commitment || *commitment != bundle.mail_operation.authorization.payload_commitment) {
        return EvidenceVerificationError::INVALID_OPERATION_SIGNATURE;
    }

    // Verify device authorization signature
    const auto op_digest = ComputeDeviceOperationDigest(bundle.network_id, bundle.mail_operation.authorization);
    if (!op_digest || !VerifyIdentityMessage(bundle.sender_device_key, bundle.mail_operation.authorization.signature, *op_digest)) {
        return EvidenceVerificationError::INVALID_OPERATION_SIGNATURE;
    }

    // Verify transaction inclusion in block
    const ProtocolOperation proto_op{bundle.mail_operation};
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
    const MailEvidenceBundle& bundle,
    const uint256& content_commitment)
{
    return content_commitment == bundle.mail_operation.mail.content_commitment;
}

std::optional<MailEvidenceBundle> CreateMailEvidenceBundle(
    const CybouBlock& block,
    size_t operation_index,
    BftFinalityCertificate finality_certificate,
    IdentityHybridPublicKey sender_device_key,
    const uint256& network_id)
{
    if (operation_index >= block.operations.size()) {
        return std::nullopt;
    }

    const auto& op = block.operations[operation_index];
    if (!std::holds_alternative<AuthorizedMail>(op)) {
        return std::nullopt;
    }

    const auto& mail_op = std::get<AuthorizedMail>(op);

    OperationInclusionProof proof;
    proof.operation_index = static_cast<uint32_t>(operation_index);
    proof.operation_hashes.reserve(block.operations.size());
    for (const auto& item : block.operations) {
        const auto serialized = SerializeProtocolOperation(item);
        if (!serialized) return std::nullopt;
        uint256 h;
        CSHA256().Write(serialized->data(), serialized->size()).Finalize(h.begin());
        proof.operation_hashes.push_back(h);
    }

    CybouBlockHeader header = ExtractBlockHeader(block);

    return MailEvidenceBundle{
        .version = CYBOU_MAIL_EVIDENCE_VERSION,
        .network_id = network_id,
        .mail_operation = mail_op,
        .block_header = std::move(header),
        .inclusion_proof = std::move(proof),
        .finality_certificate = std::move(finality_certificate),
        .sender_device_key = std::move(sender_device_key),
    };
}

std::optional<std::vector<unsigned char>> SerializeMailEvidenceBundle(const MailEvidenceBundle& bundle)
{
    std::vector<unsigned char> out;
    out.push_back(bundle.version);
    out.insert(out.end(), bundle.network_id.begin(), bundle.network_id.end());

    // mail_operation
    const auto serialized_mail = SerializeAuthorizedMail(bundle.mail_operation);
    if (!serialized_mail) return std::nullopt;
    AppendUint32LE(out, static_cast<uint32_t>(serialized_mail->size()));
    out.insert(out.end(), serialized_mail->begin(), serialized_mail->end());

    // block_header: version (1), parent (32), height (8), ops_root (32), state_root (32)
    out.push_back(bundle.block_header.version);
    out.insert(out.end(), bundle.block_header.parent_block_id.begin(), bundle.block_header.parent_block_id.end());
    AppendUint64LE(out, bundle.block_header.height);
    out.insert(out.end(), bundle.block_header.operations_root.begin(), bundle.block_header.operations_root.end());
    out.insert(out.end(), bundle.block_header.resulting_state_root.begin(), bundle.block_header.resulting_state_root.end());

    // inclusion_proof: operation_index (4), count (4), hashes (count * 32)
    AppendUint32LE(out, bundle.inclusion_proof.operation_index);
    const uint32_t hash_count = static_cast<uint32_t>(bundle.inclusion_proof.operation_hashes.size());
    AppendUint32LE(out, hash_count);
    for (const auto& h : bundle.inclusion_proof.operation_hashes) {
        out.insert(out.end(), h.begin(), h.end());
    }

    // finality_certificate
    const auto serialized_cert = SerializeFinalityCertificate(bundle.finality_certificate);
    if (!serialized_cert) return std::nullopt;
    AppendUint32LE(out, static_cast<uint32_t>(serialized_cert->size()));
    out.insert(out.end(), serialized_cert->begin(), serialized_cert->end());

    // sender_device_key: ed25519 (32) + ml_dsa (1312)
    if (bundle.sender_device_key.ml_dsa.size() != 1312) return std::nullopt;
    out.insert(out.end(), bundle.sender_device_key.ed25519.begin(), bundle.sender_device_key.ed25519.end());
    out.insert(out.end(), bundle.sender_device_key.ml_dsa.begin(), bundle.sender_device_key.ml_dsa.end());

    return out;
}

std::optional<MailEvidenceBundle> DeserializeMailEvidenceBundle(std::span<const unsigned char> bytes)
{
    // Minimal header: version (1) + network_id (32) + op_len (4)
    if (bytes.size() < 1 + 32 + 4) return std::nullopt;
    if (bytes[0] != CYBOU_MAIL_EVIDENCE_VERSION) return std::nullopt;

    uint256 network_id;
    std::copy_n(bytes.begin() + 1, 32, network_id.begin());

    size_t offset = 1 + 32;

    // mail_operation
    const uint32_t op_len = ReadUint32LE(bytes, offset);
    offset += 4;
    if (bytes.size() < offset + op_len) return std::nullopt;

    auto auth_mail = DeserializeAuthorizedMail(bytes.subspan(offset, op_len));
    if (!auth_mail.has_value()) return std::nullopt;
    offset += op_len;

    // block_header: version (1) + parent (32) + height (8) + ops_root (32) + state_root (32) = 105 bytes
    static constexpr size_t HEADER_SIZE{1 + 32 + 8 + 32 + 32};
    if (bytes.size() < offset + HEADER_SIZE) return std::nullopt;

    CybouBlockHeader header;
    header.version = bytes[offset++];
    std::copy_n(bytes.begin() + offset, 32, header.parent_block_id.begin());
    offset += 32;

    header.height = ReadUint64LE(bytes, offset);
    offset += 8;

    std::copy_n(bytes.begin() + offset, 32, header.operations_root.begin());
    offset += 32;

    std::copy_n(bytes.begin() + offset, 32, header.resulting_state_root.begin());
    offset += 32;

    // inclusion_proof: operation_index (4) + count (4) + count * 32
    if (bytes.size() < offset + 8) return std::nullopt;
    const uint32_t op_index = ReadUint32LE(bytes, offset);
    offset += 4;

    const uint32_t hash_count = ReadUint32LE(bytes, offset);
    offset += 4;

    if (bytes.size() < offset + static_cast<size_t>(hash_count) * 32) return std::nullopt;
    OperationInclusionProof proof;
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
    const uint32_t cert_len = ReadUint32LE(bytes, offset);
    offset += 4;

    if (bytes.size() < offset + cert_len) return std::nullopt;
    auto cert = DeserializeFinalityCertificate(bytes.subspan(offset, cert_len));
    if (!cert.has_value()) return std::nullopt;
    offset += cert_len;

    // sender_device_key: ed25519 (32) + ml_dsa (1312) = 1344 bytes
    static constexpr size_t KEY_SIZE{32 + 1312};
    if (bytes.size() != offset + KEY_SIZE) return std::nullopt;

    IdentityHybridPublicKey sender_key;
    sender_key.purpose = IdentityKeyPurpose::DEVICE;
    std::copy_n(bytes.begin() + offset, 32, sender_key.ed25519.begin());
    offset += 32;
    sender_key.ml_dsa.assign(bytes.begin() + offset, bytes.end());

    return MailEvidenceBundle{
        .version = bytes[0],
        .network_id = network_id,
        .mail_operation = std::move(*auth_mail),
        .block_header = std::move(header),
        .inclusion_proof = std::move(proof),
        .finality_certificate = std::move(*cert),
        .sender_device_key = std::move(sender_key),
    };
}

} // namespace cybou
