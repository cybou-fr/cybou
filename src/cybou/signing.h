// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_SIGNING_H
#define CYBOU_SIGNING_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cybou {

/**
 * Per-object signing domains. A signature is always computed over
 * ObjectSigningDomainTag(domain) || canonical(object), so a valid signature
 * for one protocol object can never be replayed as another object type.
 * These are distinct from OperatorKeyDomain tags, which separate keys.
 */
enum class ObjectSigningDomain : uint8_t {
    INVITE_VOUCHER,
    VALIDATOR_ADMISSION,
    VALIDATOR_REMOVAL,
    PROTOCOL_PARAMETER,
};

std::string_view ObjectSigningDomainTag(ObjectSigningDomain domain);

/**
 * Frozen signature suite identifiers. The suite fixes both algorithms and
 * their combination rule; new suites get new identifiers, never a reinterpretation.
 */
enum class SignatureSuiteId : uint16_t {
    HYBRID_ED25519_MLDSA65_V1 = 1,
};

inline constexpr size_t ED25519_SIGNATURE_SIZE{64};
inline constexpr size_t MLDSA65_SIGNATURE_SIZE{3309};

/**
 * Hybrid signature bundle for HYBRID_ED25519_MLDSA65_V1.
 *
 * Both signatures independently cover the same domain-separated canonical
 * message. The bundle is valid only if BOTH verify (AND, not OR). Fixed-size
 * fields: no arbitrary-length vectors on any consensus-adjacent path.
 */
struct SignatureBundleV1 {
    SignatureSuiteId suite_id{SignatureSuiteId::HYBRID_ED25519_MLDSA65_V1};
    std::array<unsigned char, ED25519_SIGNATURE_SIZE> classical_signature{};
    std::array<unsigned char, MLDSA65_SIGNATURE_SIZE> pq_signature{};
};

/** Structural presence check: false when both signature parts are all zero. */
bool IsPresent(const SignatureBundleV1& bundle);

} // namespace cybou

#endif // CYBOU_SIGNING_H
