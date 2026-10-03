// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Стабильный 32-байтовый идентификатор аккаунта, независимый от ротации ключей.

#ifndef CYBOU_ACCOUNT_ID_H
#define CYBOU_ACCOUNT_ID_H

#include <cybou/hash256.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

namespace cybou {

/// \brief Непрозрачный стабильный идентификатор аккаунта CYBOU.
class AccountId
{
public:
    static constexpr size_t SIZE{cybou::Hash256::size()};

    AccountId() = default;
    explicit AccountId(const cybou::Hash256& value) : m_value{value} {}

    /// \brief Создаёт AccountId из канонических 32 байт, отвергая нулевое значение.
    static std::optional<AccountId> FromBytes(std::span<const unsigned char> bytes)
    {
        if (bytes.size() != SIZE) return std::nullopt;
        cybou::Hash256 value;
        std::copy(bytes.begin(), bytes.end(), value.begin());
        AccountId id{value};
        if (id.IsNull()) return std::nullopt;
        return id;
    }

    /// \brief Возвращает true только для зарезервированного нулевого идентификатора.
    bool IsNull() const { return m_value.IsNull(); }
    /// \brief Возвращает канонические 32 байта идентификатора.
    const cybou::Hash256& Value() const { return m_value; }

    friend bool operator==(const AccountId&, const AccountId&) = default;
    friend bool operator<(const AccountId& a, const AccountId& b) { return a.m_value < b.m_value; }

private:
    cybou::Hash256 m_value;
};

} // namespace cybou

#endif // CYBOU_ACCOUNT_ID_H
