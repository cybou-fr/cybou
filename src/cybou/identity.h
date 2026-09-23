// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_H
#define CYBOU_IDENTITY_H

#include <cybou/account_id.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
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

/**
 * Account authorization commitment and descriptor.
 * Represents initial key/authorization binding for permissionless account creation.
 */
struct AccountAuthorizationV1 {
    uint256 auth_key_commitment;

    friend bool operator==(const AccountAuthorizationV1&, const AccountAuthorizationV1&) = default;
};

std::vector<unsigned char> SerializeAccountAuthorization(const AccountAuthorizationV1& auth);
uint256 ComputeAuthCommitment(const AccountAuthorizationV1& auth);

} // namespace cybou

#endif // CYBOU_IDENTITY_H
