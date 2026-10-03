// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Реализация canonical Validation-attestation и её криптографической проверки.

#include <cybou/validation_attestation.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <string_view>

namespace cybou {
namespace {

constexpr size_t BODY_SIZE{ 32 + 32 + 32 + 32};

std::optional<std::vector<unsigned char>> SerializeBody(const ValidationAttestation& attestation)
{
    if (attestation.network_binding.IsNull() ||
        attestation.operation_id.IsNull() || attestation.finalized_base_block_id.IsNull() ||
        attestation.validator_account_id.IsNull()) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(VALIDATION_ATTESTATION_SIZE);
    out.insert(out.end(), attestation.network_binding.begin(), attestation.network_binding.end());
    out.insert(out.end(), attestation.operation_id.begin(), attestation.operation_id.end());
    out.insert(out.end(), attestation.finalized_base_block_id.begin(), attestation.finalized_base_block_id.end());
    out.insert(out.end(), attestation.validator_account_id.Value().begin(), attestation.validator_account_id.Value().end());
    return out;
}

cybou::Hash256 ReadUint256(std::span<const unsigned char> bytes)
{
    cybou::Hash256 value;
    std::copy_n(bytes.begin(), 32, value.begin());
    return value;
}

} // namespace

std::optional<std::array<unsigned char, 32>> ComputeValidationAttestationDigest(
    const ValidationAttestation& attestation)
{
    constexpr std::string_view domain{"CYBOU/VALIDATION"};
    const auto body = SerializeBody(attestation);
    if (!body) return std::nullopt;
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*body}},
            digest.data())) return std::nullopt;
    return digest;
}

std::optional<std::vector<unsigned char>> SerializeValidationAttestation(const ValidationAttestation& attestation)
{
    auto out = SerializeBody(attestation);
    if (!out || attestation.signature.ml_dsa.size() != 2420) return std::nullopt;
    out->insert(out->end(), attestation.signature.ed25519.begin(), attestation.signature.ed25519.end());
    out->insert(out->end(), attestation.signature.ml_dsa.begin(), attestation.signature.ml_dsa.end());
    return out;
}

std::optional<ValidationAttestation> DeserializeValidationAttestation(std::span<const unsigned char> bytes)
{
    if (bytes.size() != VALIDATION_ATTESTATION_SIZE) return std::nullopt;
    const auto account = AccountId::FromBytes(bytes.subspan(96, 32));
    if (!account) return std::nullopt;
    ValidationAttestation attestation{
        .network_binding = ReadUint256(bytes.subspan(0, 32)),
        .operation_id = ReadUint256(bytes.subspan(32, 32)),
        .finalized_base_block_id = ReadUint256(bytes.subspan(64, 32)),
        .validator_account_id = *account,
    };
    std::copy_n(bytes.begin() + BODY_SIZE, 64, attestation.signature.ed25519.begin());
    attestation.signature.ml_dsa.assign(bytes.begin() + BODY_SIZE + 64, bytes.end());
    if (!SerializeBody(attestation)) return std::nullopt;
    return attestation;
}

bool IsValidationEligible(const CybouState& finalized_state, const AccountId& account)
{
    const auto found = finalized_state.accounts.find(account);
    return found != finalized_state.accounts.end() &&
        found->second.authority > VALIDATION_AUTHORITY_THRESHOLD;
}

ValidationAttestationError VerifyValidationAttestation(const ValidationAttestation& attestation,
    const cybou::Hash256& network_binding, const cybou::Hash256& finalized_tip, const CybouState& finalized_state)
{
    const auto digest = ComputeValidationAttestationDigest(attestation);
    if (!digest || attestation.signature.ml_dsa.size() != 2420) return ValidationAttestationError::INVALID_PAYLOAD;
    if (attestation.network_binding != network_binding) return ValidationAttestationError::WRONG_NETWORK;
    if (attestation.finalized_base_block_id != finalized_tip) return ValidationAttestationError::STALE_BASE;
    if (!IsValidationEligible(finalized_state, attestation.validator_account_id)) {
        return ValidationAttestationError::NOT_ELIGIBLE;
    }
    const auto* record = finalized_state.identities.Find(attestation.validator_account_id);
    if (!record || !VerifyIdentityMessage(record->authorization_key, attestation.signature, *digest)) {
        return ValidationAttestationError::INVALID_SIGNATURE;
    }
    return ValidationAttestationError::NONE;
}

std::optional<ValidationAttestation> SignValidationAttestation(const ValidationSigner& signer,
    const cybou::Hash256& network_binding, const cybou::Hash256& operation_id, const cybou::Hash256& finalized_tip,
    const CybouState& finalized_state)
{
    const auto account = signer.Account();
    if (!account || !IsValidationEligible(finalized_state, *account)) return std::nullopt;
    ValidationAttestation attestation{
        .network_binding = network_binding,
        .operation_id = operation_id,
        .finalized_base_block_id = finalized_tip,
        .validator_account_id = *account,
    };
    const auto digest = ComputeValidationAttestationDigest(attestation);
    if (!digest) return std::nullopt;
    const auto signature = signer.SignAuthorization(*digest);
    if (!signature) return std::nullopt;
    attestation.signature = *signature;
    if (VerifyValidationAttestation(attestation, network_binding, finalized_tip, finalized_state) !=
        ValidationAttestationError::NONE) return std::nullopt;
    return attestation;
}

} // namespace cybou
