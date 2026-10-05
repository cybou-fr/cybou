// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Граница подписания локальной Identity узла: приватный материал не покидает реализацию.

#ifndef CYBOU_IDENTITY_SIGNER_H
#define CYBOU_IDENTITY_SIGNER_H

#include <cybou/account_id.h>
#include <cybou/identity_crypto.h>

#include <memory>
#include <optional>
#include <span>

namespace cybou {

/// \brief Разблокированная Identity узла; сейчас подписывает только StoragePayoutBinding (DEC-282).
class IdentitySigner {
public:
    virtual ~IdentitySigner() = default;
    /// \brief AccountID активной Identity или std::nullopt, если Identity недоступна.
    virtual std::optional<AccountId> Account() const = 0;
    /// \brief Подписывает 32-байтовый digest текущим Authorization-ключом.
    virtual std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const = 0;
};

using IdentitySignerRef = std::shared_ptr<IdentitySigner>;

} // namespace cybou

#endif // CYBOU_IDENTITY_SIGNER_H
