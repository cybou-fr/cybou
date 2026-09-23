// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>

#include <crypto/sha256.h>

#include <cassert>

namespace cybou {

std::string_view KeyDomainTag(const OperatorKeyDomain domain)
{
    switch (domain) {
    case OperatorKeyDomain::AUTHORITY:
        return "CYBOU/OPERATOR-AUTHORITY/V1";
    case OperatorKeyDomain::VALIDATOR:
        return "CYBOU/OPERATOR-VALIDATOR/V1";
    case OperatorKeyDomain::RELEASE_SIGNING:
        return "CYBOU/RELEASE-SIGNING/V1";
    case OperatorKeyDomain::TREASURY:
        return "CYBOU/TREASURY/V1";
    }
    assert(false);
    return {};
}

std::vector<unsigned char> SerializeAccountAuthorization(const AccountAuthorizationV1& auth)
{
    std::vector<unsigned char> out;
    out.reserve(uint256::size());
    out.insert(out.end(), auth.auth_key_commitment.begin(), auth.auth_key_commitment.end());
    return out;
}

uint256 ComputeAuthCommitment(const AccountAuthorizationV1& auth)
{
    static constexpr std::string_view DOMAIN{"CYBOU/AUTH-COMMITMENT/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(auth.auth_key_commitment.begin(), uint256::size());
    uint256 result;
    hasher.Finalize(result.begin());
    return result;
}

} // namespace cybou
