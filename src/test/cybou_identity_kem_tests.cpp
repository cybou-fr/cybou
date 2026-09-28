// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_kem.h>
#include <cybou/crypto/sha256.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>

BOOST_AUTO_TEST_SUITE(cybou_identity_kem_tests)

BOOST_AUTO_TEST_CASE(xwing_seed_derives_published_hybrid_key_and_round_trips)
{
    const auto seed = cybou::GenerateXWingSeed();
    BOOST_REQUIRE(seed);
    BOOST_CHECK(cybou::ValidateXWingKeyPair(*seed));
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

#if defined(CYBOU_ENABLE_TEST_HOOKS)
BOOST_AUTO_TEST_CASE(xwing_draft04_zero_seed_deterministic_encapsulation_vector)
{
    // draft-irtf-cfrg-concrete-hybrid-kems-04, Appendix B.2.
    const cybou::XWingSeed seed{};
    std::array<unsigned char, 64> randomness{};
    randomness.fill(0x64);
    const auto public_key = cybou::DeriveXWingPublicKey(seed);
    BOOST_REQUIRE(public_key);
    const auto encapsulated = cybou::EncapsulateXWingForTest(*public_key, randomness);
    BOOST_REQUIRE(encapsulated);

    std::array<unsigned char, 32> public_key_digest{};
    std::array<unsigned char, 32> ciphertext_digest{};
    BOOST_REQUIRE(cybou::crypto::ComputeSha256({*public_key}, public_key_digest.data()));
    BOOST_REQUIRE(cybou::crypto::ComputeSha256({encapsulated->ciphertext}, ciphertext_digest.data()));
    static constexpr std::array<unsigned char, 32> EXPECTED_PUBLIC_KEY_DIGEST{
        0x3b, 0xb0, 0xb0, 0x03, 0xf5, 0x53, 0xf4, 0x9f,
        0x38, 0xba, 0xb3, 0x15, 0x46, 0xb7, 0xf4, 0xfd,
        0xd3, 0x23, 0xc7, 0x4c, 0xbf, 0x4a, 0xaf, 0x97,
        0xd9, 0x70, 0x3e, 0xde, 0x4e, 0x83, 0xef, 0xf7};
    static constexpr std::array<unsigned char, 32> EXPECTED_CIPHERTEXT_DIGEST{
        0x89, 0xef, 0x4c, 0xcf, 0x41, 0x48, 0x3c, 0x6f,
        0xa6, 0x27, 0x44, 0xd2, 0x3a, 0x0f, 0xd6, 0x07,
        0x2a, 0x2e, 0x63, 0x7f, 0x74, 0x56, 0x4d, 0xb7,
        0xf6, 0x18, 0xd1, 0x51, 0x6e, 0xd8, 0xa6, 0xcd};
    static constexpr cybou::XWingSharedSecret EXPECTED_SHARED_SECRET{
        0xe5, 0xba, 0x94, 0x03, 0x1e, 0xa6, 0xef, 0xd6,
        0x9c, 0x09, 0xc2, 0x54, 0xf6, 0xd9, 0x78, 0x31,
        0x36, 0xba, 0x60, 0x37, 0xe2, 0xd4, 0xc4, 0x3b,
        0xcc, 0xcf, 0x19, 0xd6, 0xf3, 0xf4, 0x34, 0x3a};
    BOOST_CHECK(public_key_digest == EXPECTED_PUBLIC_KEY_DIGEST);
    BOOST_CHECK(ciphertext_digest == EXPECTED_CIPHERTEXT_DIGEST);
    BOOST_CHECK(encapsulated->shared_secret == EXPECTED_SHARED_SECRET);

    const auto decapsulated = cybou::DecapsulateXWing(seed, encapsulated->ciphertext);
    BOOST_REQUIRE(decapsulated);
    BOOST_CHECK(*decapsulated == EXPECTED_SHARED_SECRET);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
