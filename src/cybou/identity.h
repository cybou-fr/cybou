// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_H
#define CYBOU_IDENTITY_H

#include <cybou/signing.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace cybou {

/** Security domains whose keys must never be interchangeable. */
enum class OperatorKeyDomain : uint8_t {
    AUTHORITY,
    VALIDATOR,
    RELEASE_SIGNING,
    TREASURY,
};

std::string_view KeyDomainTag(OperatorKeyDomain domain);

inline constexpr uint64_t WELCOME_GRANT{6000};

/**
 * Canonical Invite Voucher payload, version 1.
 *
 * The signed bytes are defined exclusively by SerializeInviteVoucherPayload:
 * fixed field order, uint64 little-endian, 32-byte hashes in internal byte
 * order, no dynamic-length fields. The C++ struct layout is irrelevant to
 * the wire/consensus format.
 *
 * network_id binds the voucher to one chain (anti cross-chain replay).
 * beneficiary_account_id binds it to one account (anti bearer interception).
 */
struct InviteVoucherPayloadV1 {
    uint256 network_id;
    uint256 voucher_id;
    uint256 beneficiary_account_id;
    uint64_t grant_amount{WELCOME_GRANT};
    uint64_t expiry_epoch{0};
    std::optional<uint256> organization_id;
};

/** Canonical size without organization_id: 3*32 + 8 + 8 + 1. */
inline constexpr size_t INVITE_VOUCHER_PAYLOAD_BASE_SIZE{113};

std::vector<unsigned char> SerializeInviteVoucherPayload(const InviteVoucherPayloadV1& payload);

/** Domain-separated signing message: tag || canonical payload. */
std::vector<unsigned char> InviteVoucherSigMessage(const InviteVoucherPayloadV1& payload);

struct InviteVoucher {
    InviteVoucherPayloadV1 payload;
    SignatureBundleV1 signature;
};

struct InviteVoucherValidationContext {
    uint256 expected_network_id;
    uint256 redeemer_account_id;
    uint64_t current_epoch{0};
    // DEV-ONLY seam: real hybrid Ed25519+ML-DSA-65 verification replaces this
    // caller-supplied flag in v0.0.3.2. It must never enter a consensus path.
    bool operator_authority_signature_valid{false};
    bool voucher_already_consumed{false};
};

enum class InviteVoucherError : uint8_t {
    NONE,
    NULL_VOUCHER_ID,
    NULL_BENEFICIARY,
    NETWORK_MISMATCH,
    BENEFICIARY_MISMATCH,
    INVALID_GRANT_AMOUNT,
    EXPIRED,
    NULL_ORGANIZATION_ID,
    MISSING_AUTHORITY_SIGNATURE,
    INVALID_AUTHORITY_SIGNATURE,
    ALREADY_CONSUMED,
};

/**
 * Validate the Invite Voucher envelope.
 *
 * Cryptographic verification under the Operator Authority domain is a
 * separate step over InviteVoucherSigMessage(); structural validity alone
 * can never authorize a Welcome Grant.
 */
InviteVoucherError ValidateInviteVoucher(
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context);

} // namespace cybou

#endif // CYBOU_IDENTITY_H
