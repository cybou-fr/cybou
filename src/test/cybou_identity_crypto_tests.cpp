// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_crypto.h>

#include <boost/test/unit_test.hpp>
#include <openssl/sha.h>
#include <util/strencodings.h>

#include <array>
#include <string_view>

BOOST_AUTO_TEST_SUITE(cybou_identity_crypto_tests)

BOOST_AUTO_TEST_CASE(hybrid_root_and_device_are_deterministic_and_both_required)
{
    std::array<unsigned char, 32> secret{};
    for (size_t i{0}; i < secret.size(); ++i) secret[i] = static_cast<unsigned char>(i);
    constexpr std::string_view text{"CYBOU/IDENTITY-V2/TEST"};
    const auto message = std::span<const unsigned char>{reinterpret_cast<const unsigned char*>(text.data()), text.size()};
    for (const auto purpose : {cybou::IdentityKeyPurpose::RECOVERY_ROOT, cybou::IdentityKeyPurpose::DEVICE}) {
        const auto key = cybou::DeriveIdentityPublicKey(secret, purpose);
        const auto again = cybou::DeriveIdentityPublicKey(secret, purpose);
        BOOST_REQUIRE(key && again);
        BOOST_CHECK(key->ed25519 == again->ed25519);
        BOOST_CHECK(key->ml_dsa == again->ml_dsa);
        std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
        SHA256(key->ml_dsa.data(), key->ml_dsa.size(), digest.data());
        const std::string_view expected = purpose == cybou::IdentityKeyPurpose::RECOVERY_ROOT
            ? "f06127c8c8fd51c9597f84d1a21751fa5fe616090d48af934b79f15a93bb7c60"
            : "6379ebbaf5a9bfe23d27a81c06821f76b93d8a8741b4b88fd42db415489a3ff2";
        BOOST_CHECK_EQUAL(HexStr(digest), expected);
        const auto signature = cybou::SignIdentityMessage(secret, purpose, message);
        BOOST_REQUIRE(signature);
        BOOST_CHECK(cybou::VerifyIdentityMessage(*key, *signature, message));
        auto bad_classical = *signature;
        bad_classical.ed25519[0] ^= 1;
        BOOST_CHECK(!cybou::VerifyIdentityMessage(*key, bad_classical, message));
        auto bad_pq = *signature;
        bad_pq.ml_dsa[0] ^= 1;
        BOOST_CHECK(!cybou::VerifyIdentityMessage(*key, bad_pq, message));
        std::array<unsigned char, 1> different{42};
        BOOST_CHECK(!cybou::VerifyIdentityMessage(*key, *signature, different));
    }
}

BOOST_AUTO_TEST_CASE(recovery_key_id_binds_both_public_keys_and_suite)
{
    std::array<unsigned char, 32> entropy{};
    for (size_t i{0}; i < entropy.size(); ++i) entropy[i] = static_cast<unsigned char>(i);
    const auto root = cybou::DeriveIdentityPublicKey(entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(root);
    const auto id = cybou::ComputeRecoveryKeyId(*root);
    BOOST_REQUIRE(id);
    BOOST_CHECK_EQUAL(HexStr(*id), "1ef6e05c9f58218d3bd21f3ae7aeb96ed028e92712d3b85af43d976fbf4a014c");
    BOOST_CHECK(cybou::ComputeRecoveryKeyId(*root) == id);

    auto altered_ed = *root;
    altered_ed.ed25519[0] ^= 1;
    BOOST_CHECK(cybou::ComputeRecoveryKeyId(altered_ed) != id);
    auto altered_pq = *root;
    altered_pq.ml_dsa[0] ^= 1;
    BOOST_CHECK(cybou::ComputeRecoveryKeyId(altered_pq) != id);
    auto wrong_suite = *root;
    wrong_suite.purpose = cybou::IdentityKeyPurpose::DEVICE;
    BOOST_CHECK(!cybou::ComputeRecoveryKeyId(wrong_suite));
    auto missing_key = *root;
    missing_key.ml_dsa.clear();
    BOOST_CHECK(!cybou::ComputeRecoveryKeyId(missing_key));
    missing_key = *root;
    missing_key.ed25519.fill(0);
    BOOST_CHECK(!cybou::ComputeRecoveryKeyId(missing_key));
}

BOOST_AUTO_TEST_CASE(protocol_roles_have_separate_hybrid_key_domains)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = 17;
    const std::array<unsigned char, 3> message{1, 2, 3};
    const auto root = cybou::DeriveIdentityPublicKey(seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(root);
    for (const auto purpose : {cybou::IdentityKeyPurpose::VALIDATOR,
             cybou::IdentityKeyPurpose::OPERATOR_AUTHORITY,
             cybou::IdentityKeyPurpose::RELEASE_SIGNING,
             cybou::IdentityKeyPurpose::TREASURY}) {
        const auto key = cybou::DeriveIdentityPublicKey(seed, purpose);
        const auto signature = cybou::SignIdentityMessage(seed, purpose, message);
        BOOST_REQUIRE(key && signature);
        BOOST_CHECK(key->ml_dsa.size() == 1952);
        BOOST_CHECK(signature->ml_dsa.size() == 3309);
        BOOST_CHECK(key->ed25519 != root->ed25519);
        BOOST_CHECK(key->ml_dsa != root->ml_dsa);
        BOOST_CHECK(cybou::VerifyIdentityMessage(*key, *signature, message));
        BOOST_CHECK(!cybou::VerifyIdentityMessage(*root, *signature, message));
    }
}

BOOST_AUTO_TEST_SUITE_END()
