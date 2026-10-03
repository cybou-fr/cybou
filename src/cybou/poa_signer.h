// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Абстрактная граница локального PoA signer'а без утечки приватного ключа наружу.

#ifndef CYBOU_POA_SIGNER_H
#define CYBOU_POA_SIGNER_H

#include <cybou/identity_crypto.h>

#include <memory>
#include <optional>
#include <span>

namespace cybou {

/// \brief Интерфейс локального PoA signer'а, инкапсулирующий приватный ключ.
class PoaSigner {
public:
    virtual ~PoaSigner() = default;
    /// \brief Возвращает публичный ключ signer'а для привязки к genesis-authorized PoA key.
    virtual std::optional<IdentityHybridPublicKey> PublicKey() const = 0;
    /// \brief Подписывает уже подготовленный message digest/bytes без раскрытия ключевого материала.
    virtual std::optional<IdentityHybridSignature> Sign(std::span<const unsigned char> message) const = 0;
};

using PoaSignerRef = std::shared_ptr<PoaSigner>;

} // namespace cybou

#endif // CYBOU_POA_SIGNER_H
