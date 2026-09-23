// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_H
#define CYBOU_IDENTITY_H

#include <cybou/account_id.h>
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
inline constexpr uint8_t INVITE_VOUCHER_PAYLOAD_VERSION{1};

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
    uint8_t payload_version{INVITE_VOUCHER_PAYLOAD_VERSION};
    uint256 network_id;
    uint256 voucher_id;
    AccountId beneficiary_account_id;
    uint64_t grant_amount{WELCOME_GRANT};
    uint64_t expiry_epoch{0};
    std::optional<uint256> organization_id;
};

/** Canonical size without organization_id: 1 + 3*32 + 8 + 8 + 1. */
inline constexpr size_t INVITE_VOUCHER_PAYLOAD_BASE_SIZE{114};

std::vector<unsigned char> SerializeInviteVoucherPayload(const InviteVoucherPayloadV1& payload);

/** Signing message: domain || suite_id_le16 || keyset_id || canonical payload. */
std::vector<unsigned char> InviteVoucherSigMessage(
    const InviteVoucherPayloadV1& payload,
    SignatureSuiteId suite_id,
    const uint256& authority_keyset_id);

struct InviteVoucher {
    InviteVoucherPayloadV1 payload;
    SignatureBundleV1 signature;
};

struct InviteVoucherValidationContext {
    uint256 expected_network_id;
    AccountId redeemer_account_id;
    uint64_t current_epoch{0};
    const OperatorAuthorityKeySet* authority_keyset{nullptr};
    bool voucher_already_consumed{false};
};

enum class InviteVoucherError : uint8_t {
    NONE,
    UNSUPPORTED_PAYLOAD_VERSION,
    NULL_VOUCHER_ID,
    NULL_BENEFICIARY,
    NETWORK_MISMATCH,
    BENEFICIARY_MISMATCH,
    INVALID_GRANT_AMOUNT,
    EXPIRED,
    NULL_ORGANIZATION_ID,
    MISSING_AUTHORITY_SIGNATURE,
    UNKNOWN_AUTHORITY_KEYSET,
    INACTIVE_AUTHORITY_KEYSET,
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
    const InviteVoucherValidationContext& context,
    const OperatorAuthoritySignatureVerifier& verifier);

} // namespace cybou

#endif // CYBOU_IDENTITY_H
