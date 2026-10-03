// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/signing.h>

#include <cybou/hash256.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <set>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_signing_tests)

BOOST_AUTO_TEST_CASE(user_ed25519_key_derivation_signing_and_verification)
{
    std::array<unsigned char, 32> privkey{};
    privkey.fill(0x42);

    const auto pubkey{cybou::DeriveEd25519PublicKey(privkey)};
    BOOST_REQUIRE(pubkey.has_value());
    BOOST_CHECK(!pubkey->IsNull());

    const std::string msg_str{"Hello CYBOU"};
    const std::span<const unsigned char> msg{reinterpret_cast<const unsigned char*>(msg_str.data()), msg_str.size()};

    const auto sig{cybou::SignUserMessage(privkey, msg)};
    BOOST_REQUIRE(sig.has_value());

    BOOST_CHECK(cybou::VerifyUserSignature(*pubkey, *sig, msg));

    // Tampered message
    const std::string tampered_str{"Hello CYBOU!"};
    const std::span<const unsigned char> tampered{reinterpret_cast<const unsigned char*>(tampered_str.data()), tampered_str.size()};
    BOOST_CHECK(!cybou::VerifyUserSignature(*pubkey, *sig, tampered));

    // Tampered public key
    auto wrong_pub{*pubkey};
    wrong_pub.begin()[0] ^= 1;
    BOOST_CHECK(!cybou::VerifyUserSignature(wrong_pub, *sig, msg));

    // Null public key
    BOOST_CHECK(!cybou::VerifyUserSignature(cybou::Hash256{}, *sig, msg));
}

BOOST_AUTO_TEST_SUITE_END()
