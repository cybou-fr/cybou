// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BOOTSTRAP_IDENTITY_H
#define CYBOU_BOOTSTRAP_IDENTITY_H

#include <cybou/identity_crypto.h>
#include <cybou/identity_registry.h>

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

struct BootstrapIdentityClaim {
    std::array<unsigned char, 32> challenge{};
    AccountId account_id;
    IdentityHybridPublicKey recovery_key{.purpose = IdentityKeyPurpose::RECOVERY_ROOT,
        .ed25519 = {}, .ml_dsa = {}};
    IdentityHybridSignature proof;

    friend bool operator==(const BootstrapIdentityClaim&, const BootstrapIdentityClaim&) = default;
};

std::vector<unsigned char> BootstrapIdentityClaimMessage(std::span<const unsigned char> tls_exporter,
    std::span<const unsigned char, 32> challenge, const AccountId& account_id,
    const IdentityHybridPublicKey& recovery_key);
std::optional<std::array<unsigned char, 32>> VerifyBootstrapIdentityClaim(
    const BootstrapIdentityClaim& claim, std::span<const unsigned char> tls_exporter);
std::optional<std::vector<unsigned char>> EncodeBootstrapIdentityClaim(const BootstrapIdentityClaim& claim);
std::optional<BootstrapIdentityClaim> DecodeBootstrapIdentityClaim(std::span<const unsigned char> bytes);

} // namespace cybou
#endif
