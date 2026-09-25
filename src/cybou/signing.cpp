// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/signing.h>

#include <algorithm>
#include <cassert>

namespace cybou {

std::string_view ObjectSigningDomainTag(const ObjectSigningDomain domain)
{
    switch (domain) {
    case ObjectSigningDomain::VALIDATOR_ADMISSION:
        return "CYBOU/SIG/VALIDATOR-ADMISSION/V1";
    case ObjectSigningDomain::VALIDATOR_REMOVAL:
        return "CYBOU/SIG/VALIDATOR-REMOVAL/V1";
    case ObjectSigningDomain::PROTOCOL_PARAMETER:
        return "CYBOU/SIG/PROTOCOL-PARAMETER/V1";
    }
    assert(false);
    return {};
}

bool IsPresent(const SignatureBundle& bundle)
{
    const auto nonzero = [](const auto& sig) {
        return std::ranges::any_of(sig, [](unsigned char b) { return b != 0; });
    };
    return bundle.suite_id == SignatureSuiteId::HYBRID_ED25519_MLDSA65 &&
        !bundle.authority_keyset_id.IsNull() &&
        nonzero(bundle.classical_signature) && nonzero(bundle.pq_signature);
}

bool IsActiveAtEpoch(const OperatorAuthorityKeySet& keyset, const uint64_t epoch)
{
    return epoch >= keyset.active_from_epoch &&
        (!keyset.retired_from_epoch || epoch < *keyset.retired_from_epoch);
}

std::vector<unsigned char> SerializeSignatureBundle(const SignatureBundle& bundle)
{
    std::vector<unsigned char> out;
    out.reserve(SIGNATURE_BUNDLE_SIZE);
    const auto suite_val = static_cast<uint16_t>(bundle.suite_id);
    out.push_back(static_cast<unsigned char>(suite_val & 0xff));
    out.push_back(static_cast<unsigned char>((suite_val >> 8) & 0xff));
    out.insert(out.end(), bundle.authority_keyset_id.begin(), bundle.authority_keyset_id.end());
    out.insert(out.end(), bundle.classical_signature.begin(), bundle.classical_signature.end());
    out.insert(out.end(), bundle.pq_signature.begin(), bundle.pq_signature.end());
    return out;
}

std::optional<SignatureBundle> DeserializeSignatureBundle(const std::span<const unsigned char> bytes)
{
    if (bytes.size() != SIGNATURE_BUNDLE_SIZE) return std::nullopt;
    const uint16_t suite_val = static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
    SignatureBundle bundle;
    bundle.suite_id = static_cast<SignatureSuiteId>(suite_val);
    std::copy_n(bytes.begin() + 2, 32, bundle.authority_keyset_id.begin());
    std::copy_n(bytes.begin() + 34, ED25519_SIGNATURE_SIZE, bundle.classical_signature.begin());
    std::copy_n(bytes.begin() + 34 + ED25519_SIGNATURE_SIZE, MLDSA65_SIGNATURE_SIZE, bundle.pq_signature.begin());
    return bundle;
}

} // namespace cybou
