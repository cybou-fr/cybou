// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/root_publication.h>


#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cybou_root_publication_capsule_tests)

BOOST_AUTO_TEST_CASE(root_recipient_capsule_wraps_content_key_and_binds_context)
{
    std::array<unsigned char, 32> network{};
    std::array<unsigned char, 32> sender{};
    cybou::ChunkId root{};
    cybou::ContentKey content_key{};
    network.fill(0x31);
    sender.fill(0x42);
    root.fill(0x53);
    content_key.fill(0x64);

    const auto seed = cybou::GenerateXWingSeed();
    BOOST_REQUIRE(seed.has_value());
    const auto public_key = cybou::DeriveXWingPublicKey(*seed);
    BOOST_REQUIRE(public_key.has_value());
    const auto capsule = cybou::CreateRootRecipientCapsule(
        network, sender, 7, 2, root, *public_key, 4, content_key);
    BOOST_REQUIRE(capsule.has_value());

    const auto opened = cybou::OpenRootRecipientCapsule(
        network, sender, 7, 2, root, *capsule, *seed);
    BOOST_REQUIRE(opened.has_value());
    BOOST_CHECK(*opened == content_key);

    auto wrong_network = network;
    wrong_network[0] ^= 1;
    BOOST_CHECK(!cybou::OpenRootRecipientCapsule(
        wrong_network, sender, 7, 2, root, *capsule, *seed));
    auto wrong_root = root;
    wrong_root[0] ^= 1;
    BOOST_CHECK(!cybou::OpenRootRecipientCapsule(
        network, sender, 7, 2, wrong_root, *capsule, *seed));
    auto wrong_epoch = *capsule;
    ++wrong_epoch.key_epoch;
    BOOST_CHECK(!cybou::OpenRootRecipientCapsule(
        network, sender, 7, 2, root, wrong_epoch, *seed));
    auto tampered = *capsule;
    tampered.wrapped_content_key.back() ^= 1;
    BOOST_CHECK(!cybou::OpenRootRecipientCapsule(
        network, sender, 7, 2, root, tampered, *seed));
}

BOOST_AUTO_TEST_SUITE_END()
