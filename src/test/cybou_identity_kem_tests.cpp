// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_kem.h>
#include <cybou/crypto/sha256.h>

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

BOOST_AUTO_TEST_CASE(xwing_seed_derives_published_hybrid_key_and_round_trips)
{
    const auto seed = cybou::GenerateXWingSeed();
    BOOST_REQUIRE(seed);
    const auto public_key = cybou::DeriveXWingPublicKey(*seed);
    BOOST_REQUIRE(public_key);
    BOOST_CHECK(cybou::DeriveXWingPublicKey(*seed) == public_key);
    BOOST_CHECK(!std::all_of(public_key->begin(), public_key->end(), [](unsigned char byte) { return byte == 0; }));
    const auto encapsulated = cybou::EncapsulateXWing(*public_key);
    BOOST_REQUIRE(encapsulated);
    const auto decapsulated = cybou::DecapsulateXWing(*seed, encapsulated->ciphertext);
    BOOST_REQUIRE(decapsulated);
    BOOST_CHECK(*decapsulated == encapsulated->shared_secret);
}

BOOST_AUTO_TEST_CASE(xwing_draft03_zero_seed_public_key_vector)
{
    // draft-irtf-cfrg-concrete-hybrid-kems-03, Appendix A.2.
    const cybou::XWingSeed seed{};
    const auto public_key = cybou::DeriveXWingPublicKey(seed);
    BOOST_REQUIRE(public_key);
    std::array<unsigned char, 32> digest{};
    BOOST_REQUIRE(cybou::crypto::ComputeSha256({*public_key}, digest.data()));
    static constexpr std::array<unsigned char, 32> EXPECTED_DIGEST{
        0x3b, 0xb0, 0xb0, 0x03, 0xf5, 0x53, 0xf4, 0x9f,
        0x38, 0xba, 0xb3, 0x15, 0x46, 0xb7, 0xf4, 0xfd,
        0xd3, 0x23, 0xc7, 0x4c, 0xbf, 0x4a, 0xaf, 0x97,
        0xd9, 0x70, 0x3e, 0xde, 0x4e, 0x83, 0xef, 0xf7};
    BOOST_CHECK(digest == EXPECTED_DIGEST);
    static constexpr std::array<unsigned char, 32> EXPECTED_X25519_PUBLIC{
        0xf6, 0x36, 0x01, 0xb7, 0xf8, 0x5a, 0xcc, 0xfe,
        0xea, 0x2d, 0x17, 0x96, 0x4c, 0x66, 0xb5, 0x19,
        0x4b, 0x0f, 0x08, 0xe1, 0x85, 0x19, 0xfa, 0xae,
        0xe1, 0x94, 0xe3, 0xc1, 0x02, 0x82, 0x30, 0x62};
    BOOST_CHECK(std::equal(EXPECTED_X25519_PUBLIC.begin(), EXPECTED_X25519_PUBLIC.end(),
        public_key->end() - EXPECTED_X25519_PUBLIC.size()));
}

BOOST_AUTO_TEST_SUITE_END()
