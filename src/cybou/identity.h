// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_H
#define CYBOU_IDENTITY_H

#include <cybou/account_id.h>
#include <cybou/identity_authorization.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cybou {

/** Security domains whose keys must never be interchangeable. */
enum class OperatorKeyDomain : uint8_t {
    AUTHORITY,
    VALIDATOR,
    RELEASE_SIGNING,
    TREASURY,
};

std::string_view KeyDomainTag(OperatorKeyDomain domain);

} // namespace cybou

#endif // CYBOU_IDENTITY_H
