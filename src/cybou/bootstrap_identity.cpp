// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_identity.h>

#include <algorithm>
#include <string_view>

namespace cybou {

std::vector<unsigned char> BootstrapIdentityClaimMessage(const std::span<const unsigned char> tls_exporter,
    const std::span<const unsigned char, 32> challenge, const AccountId& account_id,
    const IdentityHybridPublicKey& recovery_key)
{
    if (tls_exporter.size() != 32 || account_id.IsNull() ||
        recovery_key.purpose != IdentityKeyPurpose::RECOVERY_ROOT ||
        recovery_key.ml_dsa.size() != 1952) return {};
    constexpr std::string_view DOMAIN{"CYBOU/PREGENESIS-BOOTSTRAP-IDENTITY/v1"};
    std::vector<unsigned char> message(DOMAIN.begin(), DOMAIN.end());
    message.insert(message.end(), challenge.begin(), challenge.end());
    message.insert(message.end(), tls_exporter.begin(), tls_exporter.end());
    message.insert(message.end(), account_id.Value().begin(), account_id.Value().end());
    message.insert(message.end(), recovery_key.ed25519.begin(), recovery_key.ed25519.end());
    message.insert(message.end(), recovery_key.ml_dsa.begin(), recovery_key.ml_dsa.end());
    return message;
}

std::optional<std::array<unsigned char, 32>> VerifyBootstrapIdentityClaim(
    const BootstrapIdentityClaim& claim, const std::span<const unsigned char> tls_exporter)
{
    if (claim.account_id.IsNull() ||
        std::ranges::all_of(claim.challenge, [](unsigned char byte) { return byte == 0; })) return std::nullopt;
    const auto message = BootstrapIdentityClaimMessage(tls_exporter, claim.challenge,
        claim.account_id, claim.recovery_key);
    const auto recovery_id = ComputeRecoveryKeyId(claim.recovery_key);
    if (message.empty() || !recovery_id || !VerifyIdentityMessage(claim.recovery_key, claim.proof, message)) {
        return std::nullopt;
    }
    return recovery_id;
}

std::optional<std::vector<unsigned char>> EncodeBootstrapIdentityClaim(const BootstrapIdentityClaim& claim)
{
    if (claim.account_id.IsNull() || !ComputeRecoveryKeyId(claim.recovery_key) ||
        std::ranges::all_of(claim.challenge, [](unsigned char byte) { return byte == 0; }) ||
        claim.proof.ml_dsa.size() != 3309) return std::nullopt;
    std::vector<unsigned char> bytes{'C','Y','B','I','1'};
    bytes.insert(bytes.end(), claim.challenge.begin(), claim.challenge.end());
    bytes.insert(bytes.end(), claim.account_id.Value().begin(), claim.account_id.Value().end());
    bytes.insert(bytes.end(), claim.recovery_key.ed25519.begin(), claim.recovery_key.ed25519.end());
    bytes.insert(bytes.end(), claim.recovery_key.ml_dsa.begin(), claim.recovery_key.ml_dsa.end());
    bytes.insert(bytes.end(), claim.proof.ed25519.begin(), claim.proof.ed25519.end());
    bytes.insert(bytes.end(), claim.proof.ml_dsa.begin(), claim.proof.ml_dsa.end());
    return bytes;
}

std::optional<BootstrapIdentityClaim> DecodeBootstrapIdentityClaim(const std::span<const unsigned char> bytes)
{
    constexpr size_t ENCODED_SIZE{5 + 32 + AccountId::SIZE + 32 + 1952 + 64 + 3309};
    if (bytes.size() != ENCODED_SIZE || !std::equal(bytes.begin(), bytes.begin() + 5,
            std::array<unsigned char, 5>{'C','Y','B','I','1'}.begin())) return std::nullopt;
    size_t offset{5};
    BootstrapIdentityClaim claim;
    std::copy_n(bytes.begin() + offset, claim.challenge.size(), claim.challenge.begin());
    offset += claim.challenge.size();
    const auto account = AccountId::FromBytes(bytes.subspan(offset, AccountId::SIZE));
    if (!account) return std::nullopt;
    claim.account_id = *account;
    offset += AccountId::SIZE;
    std::copy_n(bytes.begin() + offset, claim.recovery_key.ed25519.size(), claim.recovery_key.ed25519.begin());
    offset += claim.recovery_key.ed25519.size();
    claim.recovery_key.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 1952);
    offset += 1952;
    std::copy_n(bytes.begin() + offset, claim.proof.ed25519.size(), claim.proof.ed25519.begin());
    offset += claim.proof.ed25519.size();
    claim.proof.ml_dsa.assign(bytes.begin() + offset, bytes.end());
    return ComputeRecoveryKeyId(claim.recovery_key) ? std::optional<BootstrapIdentityClaim>{std::move(claim)} : std::nullopt;
}

} // namespace cybou
