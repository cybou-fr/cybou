// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_VALIDATION_ATTESTATION_H
#define CYBOU_VALIDATION_ATTESTATION_H

#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/** Finalized AUTH must exceed this value; 1,000,000 itself is not eligible. */
inline constexpr uint64_t VALIDATION_AUTHORITY_THRESHOLD{1'000'000};
/** NetworkID || OperationID || base BlockID || AccountID || Ed25519 || ML-DSA-44 */
inline constexpr size_t VALIDATION_ATTESTATION_SIZE{ 32 + 32 + 32 + 32 + 64 + 2420};

/**
 * "This Identity's node independently executed OperationID on the finalized
 * state after base BlockID and found it valid." Stored beside the operation;
 * it never changes the OperationID and never changes state.
 */
struct ValidationAttestation {
    cybou::Hash256 network_binding;
    cybou::Hash256 operation_id;
    cybou::Hash256 finalized_base_block_id;
    AccountId validator_account_id;
    IdentityHybridSignature signature;

    friend bool operator==(const ValidationAttestation&, const ValidationAttestation&) = default;
};

enum class ValidationAttestationError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    WRONG_NETWORK,
    STALE_BASE,
    NOT_ELIGIBLE,
    INVALID_SIGNATURE,
};

std::optional<std::array<unsigned char, 32>> ComputeValidationAttestationDigest(
    const ValidationAttestation& attestation);
std::optional<std::vector<unsigned char>> SerializeValidationAttestation(const ValidationAttestation& attestation);
std::optional<ValidationAttestation> DeserializeValidationAttestation(std::span<const unsigned char> bytes);

/** Eligibility is read only from the caller's own latest finalized state. */
bool IsValidationEligible(const CybouState& finalized_state, const AccountId& account);

/**
 * Checks network, base == the caller's finalized tip, the validator's finalized
 * AUTH and its current Authorization signature. It does not check the
 * operation: the caller must have executed it independently first.
 */
ValidationAttestationError VerifyValidationAttestation(const ValidationAttestation& attestation,
    const cybou::Hash256& network_binding, const cybou::Hash256& finalized_tip, const CybouState& finalized_state);

/** Signing boundary for the local Identity; key material never leaves the implementation. */
class ValidationSigner {
public:
    virtual ~ValidationSigner() = default;
    virtual std::optional<AccountId> Account() const = 0;
    virtual std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const = 0;
};

using ValidationSignerRef = std::shared_ptr<ValidationSigner>;

/** Sign only when the local Identity is eligible in this finalized state; nullopt otherwise. */
std::optional<ValidationAttestation> SignValidationAttestation(const ValidationSigner& signer,
    const cybou::Hash256& network_binding, const cybou::Hash256& operation_id, const cybou::Hash256& finalized_tip,
    const CybouState& finalized_state);

} // namespace cybou
#endif // CYBOU_VALIDATION_ATTESTATION_H
