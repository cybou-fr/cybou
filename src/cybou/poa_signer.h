// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_POA_SIGNER_H
#define CYBOU_POA_SIGNER_H

#include <cybou/identity_crypto.h>

#include <memory>
#include <optional>
#include <span>

namespace cybou {

/** Signing boundary for local PoA capabilities. Implementations keep key material private. */
class PoaSigner {
public:
    virtual ~PoaSigner() = default;
    virtual std::optional<IdentityHybridPublicKey> PublicKey() const = 0;
    virtual std::optional<IdentityHybridSignature> Sign(std::span<const unsigned char> message) const = 0;
};

using PoaSignerRef = std::shared_ptr<PoaSigner>;

} // namespace cybou

#endif // CYBOU_POA_SIGNER_H
