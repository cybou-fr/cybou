// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/signing.h>

#include <algorithm>
#include <cassert>

namespace cybou {

std::string_view ObjectSigningDomainTag(const ObjectSigningDomain domain)
{
    switch (domain) {
    case ObjectSigningDomain::INVITE_VOUCHER:
        return "CYBOU/SIG/INVITE-VOUCHER/V1";
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

bool IsPresent(const SignatureBundleV1& bundle)
{
    const auto nonzero = [](const auto& sig) {
        return std::ranges::any_of(sig, [](unsigned char b) { return b != 0; });
    };
    return nonzero(bundle.classical_signature) || nonzero(bundle.pq_signature);
}

} // namespace cybou
