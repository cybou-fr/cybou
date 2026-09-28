#ifndef CYBOU_TEST_IDENTITY_HELPERS_H
#define CYBOU_TEST_IDENTITY_HELPERS_H

#include <cybou/account_creation.h>
#include <cybou/identity_crypto.h>

#include <stdexcept>

namespace cybou::test {

struct IdentityKemBinding {
    IdentityKemPackage package{};
    std::array<unsigned char, 32> package_id{};
    std::array<unsigned char, 32> authorization_commitment{};
    std::array<unsigned char, 32> pop_digest{};
};

inline IdentityKemBinding MakeIdentityKemBinding(const uint256& network_id,
    const AccountId& account_id, const IdentityAuthorization& authorization,
    uint64_t activation_nonce = 0)
{
    XWingSeed seed{};
    seed.fill(0x09);
    const auto public_key = DeriveXWingPublicKey(seed);
    const auto package = public_key ? EncodeIdentityKemPackage(*public_key) : std::nullopt;
    const auto device_id = ComputeDeviceKeyId(authorization.initial_device);
    const auto account_bytes = account_id.Value();
    const auto package_id = package && device_id ? ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_id.begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32}, *device_id, activation_nonce, *package) : std::nullopt;
    const auto commitment = package_id ? ComputeAccountCreateAuthorizationCommitment(authorization, *package_id) : std::nullopt;
    const auto digest = package_id ? ComputeAccountCreatePopDigest(network_id, account_id, authorization, *package_id) : std::nullopt;
    if (!package || !package_id || !commitment || !digest) throw std::runtime_error("cannot prepare Identity KEM test binding");
    return {*package, *package_id, *commitment, *digest};
}

} // namespace cybou::test

#endif
