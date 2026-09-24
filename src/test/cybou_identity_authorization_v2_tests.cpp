// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_authorization_v2.h>

#include <boost/test/unit_test.hpp>
#include <util/strencodings.h>

#include <array>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_identity_authorization_v2_tests)

BOOST_AUTO_TEST_CASE(canonical_hybrid_authorization_is_bounded_and_distinct_from_v1)
{
    std::array<unsigned char, 32> root_secret{};
    std::array<unsigned char, 32> device_secret{};
    for (size_t i{0}; i < root_secret.size(); ++i) {
        root_secret[i] = static_cast<unsigned char>(i);
        device_secret[i] = static_cast<unsigned char>(i + 32);
    }
    const auto root = cybou::DeriveIdentityPublicKey(root_secret, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = cybou::DeriveIdentityPublicKey(device_secret, cybou::IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(root && device);
    const cybou::IdentityAuthorizationV2 auth{*root, *device};
    const auto bytes = cybou::SerializeIdentityAuthorizationV2(auth);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->size(), cybou::IDENTITY_AUTHORIZATION_V2_SIZE);
    BOOST_CHECK_EQUAL((*bytes)[0], 2);
    BOOST_CHECK_EQUAL((*bytes)[1], 1);
    BOOST_CHECK_EQUAL((*bytes)[1986], 1);
    const auto decoded = cybou::DeserializeIdentityAuthorizationV2(*bytes);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(decoded->recovery_root.ed25519 == root->ed25519);
    BOOST_CHECK(decoded->recovery_root.ml_dsa == root->ml_dsa);
    BOOST_CHECK(decoded->initial_device.ed25519 == device->ed25519);
    BOOST_CHECK(decoded->initial_device.ml_dsa == device->ml_dsa);
    const auto commitment = cybou::ComputeIdentityAuthorizationCommitmentV2(auth);
    BOOST_REQUIRE(commitment);
    BOOST_CHECK_EQUAL(HexStr(*commitment), "ea14a9dce2dd3d5e7aa0051731a41335b7729759e19d05e5647c5421061cd9bb");
    BOOST_CHECK(cybou::ComputeIdentityAuthorizationCommitmentV2(*decoded) == commitment);

    auto altered = *bytes;
    altered[0] = 1;
    BOOST_CHECK(!cybou::DeserializeIdentityAuthorizationV2(altered));
    altered = *bytes;
    altered[1986] = 2;
    BOOST_CHECK(!cybou::DeserializeIdentityAuthorizationV2(altered));
    altered = *bytes;
    altered[34] ^= 1;
    const auto altered_auth = cybou::DeserializeIdentityAuthorizationV2(altered);
    BOOST_REQUIRE(altered_auth);
    BOOST_CHECK(cybou::ComputeIdentityAuthorizationCommitmentV2(*altered_auth) != commitment);
    BOOST_CHECK(!cybou::DeserializeIdentityAuthorizationV2(std::span{*bytes}.first(bytes->size() - 1)));

    auto invalid = auth;
    invalid.initial_device.ml_dsa.pop_back();
    BOOST_CHECK(!cybou::SerializeIdentityAuthorizationV2(invalid));
    invalid = auth;
    invalid.initial_device.purpose = cybou::IdentityKeyPurpose::RECOVERY_ROOT;
    BOOST_CHECK(!cybou::SerializeIdentityAuthorizationV2(invalid));
}

BOOST_AUTO_TEST_SUITE_END()
