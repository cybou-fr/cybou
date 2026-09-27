// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_kem.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>

BOOST_AUTO_TEST_SUITE(cybou_identity_kem_tests)

BOOST_AUTO_TEST_CASE(device_agreement_keys_are_independent_and_reproducible)
{
    const auto x25519_private = cybou::GenerateDeviceX25519PrivateKey();
    const auto mlkem_seed = cybou::GenerateMlKem768Seed();
    BOOST_REQUIRE(x25519_private && mlkem_seed);

    const auto x25519_public = cybou::DeriveDeviceX25519PublicKey(*x25519_private);
    const auto mlkem_public = cybou::DeriveMlKem768PublicKey(*mlkem_seed);
    BOOST_REQUIRE(x25519_public && mlkem_public);
    BOOST_CHECK(!std::all_of(x25519_public->begin(), x25519_public->end(), [](unsigned char byte) { return byte == 0; }));
    BOOST_CHECK(!std::all_of(mlkem_public->begin(), mlkem_public->end(), [](unsigned char byte) { return byte == 0; }));
    BOOST_CHECK(!cybou::DeriveDeviceX25519PublicKey(cybou::DeviceX25519PrivateKey{}));
    BOOST_CHECK(!cybou::DeriveMlKem768PublicKey(cybou::MlKem768Seed{}));

    const auto encapsulated = cybou::EncapsulateMlKem768(*mlkem_public);
    BOOST_REQUIRE(encapsulated);
    const auto decapsulated = cybou::DecapsulateMlKem768(*mlkem_seed, encapsulated->ciphertext);
    BOOST_REQUIRE(decapsulated);
    BOOST_CHECK(decapsulated->shared_secret == encapsulated->shared_secret);
    BOOST_CHECK(!cybou::EncapsulateMlKem768(cybou::MlKem768PublicKey{}));
}

BOOST_AUTO_TEST_SUITE_END()
