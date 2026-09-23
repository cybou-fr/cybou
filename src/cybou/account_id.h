// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ACCOUNT_ID_H
#define CYBOU_ACCOUNT_ID_H

#include <uint256.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

namespace cybou {

/**
 * Opaque, stable 32-byte identifier for a CYBOU account.
 *
 * AccountID identifies an authorization record; it is not itself a public key
 * and therefore remains stable across device/key rotation. All-zero is the
 * reserved invalid value. Canonical encoding is exactly the uint256 internal
 * byte order defined by the protocol.
 */
class AccountId
{
public:
    static constexpr size_t SIZE{uint256::size()};

    AccountId() = default;
    explicit AccountId(const uint256& value) : m_value{value} {}

    static std::optional<AccountId> FromBytes(std::span<const unsigned char> bytes)
    {
        if (bytes.size() != SIZE) return std::nullopt;
        uint256 value;
        std::copy(bytes.begin(), bytes.end(), value.begin());
        AccountId id{value};
        if (id.IsNull()) return std::nullopt;
        return id;
    }

    bool IsNull() const { return m_value.IsNull(); }
    const uint256& Value() const { return m_value; }

    friend bool operator==(const AccountId&, const AccountId&) = default;
    friend bool operator<(const AccountId& a, const AccountId& b) { return a.m_value < b.m_value; }

private:
    uint256 m_value;
};

} // namespace cybou

#endif // CYBOU_ACCOUNT_ID_H
