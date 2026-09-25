// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>

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

} // namespace cybou
