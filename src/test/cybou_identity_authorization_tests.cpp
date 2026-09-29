// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_authorization.h>
#include <test/cybou_test_helpers.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_identity_authorization_tests)

BOOST_AUTO_TEST_CASE(canonical_hybrid_authorization_is_bounded_and_distinct)
{
    std::array<unsigned char, 32> root_secret{};
    std::array<unsigned char, 32> authorization_secret{};
    for (size_t i{0}; i < root_secret.size(); ++i) {
        root_secret[i] = static_cast<unsigned char>(i);
        authorization_secret[i] = static_cast<unsigned char>(i + 32);
    }
    const auto root = cybou::DeriveIdentityPublicKey(root_secret, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = cybou::DeriveIdentityPublicKey(authorization_secret, cybou::IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);
    const cybou::IdentityAuthorization auth{*root, *device};
    const auto bytes = cybou::SerializeIdentityAuthorization(auth);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->size(), cybou::IDENTITY_AUTHORIZATION_SIZE);
    BOOST_CHECK_EQUAL((*bytes)[0], 3);
    BOOST_CHECK_EQUAL((*bytes)[1], 1);
    BOOST_CHECK_EQUAL((*bytes)[1986], 1);
    const auto decoded = cybou::DeserializeIdentityAuthorization(*bytes);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(decoded->recovery_root.ed25519 == root->ed25519);
    BOOST_CHECK(decoded->recovery_root.ml_dsa == root->ml_dsa);
    BOOST_CHECK(decoded->authorization_key.ed25519 == device->ed25519);
    BOOST_CHECK(decoded->authorization_key.ml_dsa == device->ml_dsa);
    const auto commitment = cybou::ComputeIdentityAuthorizationCommitment(auth);
    BOOST_REQUIRE(commitment);
    BOOST_CHECK_EQUAL(cybou::test::Hex(*commitment), "ce6cf18398b0df26a294d81e787b3d5bd6e3eb8af2b7d0bc2d61b25958334a19");
    BOOST_CHECK(cybou::ComputeIdentityAuthorizationCommitment(*decoded) == commitment);

    auto altered = *bytes;
    altered[0] = 1;
    BOOST_CHECK(!cybou::DeserializeIdentityAuthorization(altered));
    altered = *bytes;
    altered[1986] = 2;
    BOOST_CHECK(!cybou::DeserializeIdentityAuthorization(altered));
    altered = *bytes;
    altered[34] ^= 1;
    const auto altered_auth = cybou::DeserializeIdentityAuthorization(altered);
    BOOST_REQUIRE(altered_auth);
    BOOST_CHECK(cybou::ComputeIdentityAuthorizationCommitment(*altered_auth) != commitment);
    BOOST_CHECK(!cybou::DeserializeIdentityAuthorization(std::span{*bytes}.first(bytes->size() - 1)));

    auto invalid = auth;
    invalid.authorization_key.ml_dsa.pop_back();
    BOOST_CHECK(!cybou::SerializeIdentityAuthorization(invalid));
    invalid = auth;
    invalid.authorization_key.purpose = cybou::IdentityKeyPurpose::RECOVERY_ROOT;
    BOOST_CHECK(!cybou::SerializeIdentityAuthorization(invalid));
}

BOOST_AUTO_TEST_SUITE_END()
