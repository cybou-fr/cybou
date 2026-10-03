// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/encrypted_chunk.h>


#include <boost/test/unit_test.hpp>

#include <algorithm>

namespace {

std::array<unsigned char, 32> Network(const unsigned char fill)
{
    std::array<unsigned char, 32> result{};
    result.fill(fill);
    return result;
}

bool IsPaddingBucket(const std::size_t size)
{
    return size == 1024 || size == 4096 || size == 16 * 1024 ||
        size == 64 * 1024 || size == 256 * 1024 || size == 512 * 1024;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_encrypted_chunk_tests)

BOOST_AUTO_TEST_CASE(encrypted_chunk_round_trips_arbitrary_bytes)
{
    const auto network = Network(0x21);
    const auto key = cybou::GenerateContentKey();
    BOOST_REQUIRE(key.has_value());
    const std::vector<unsigned char> plaintext{0x00, 0x01, 0xff, 0x00};

    const auto encrypted = cybou::EncryptChunk(network, *key, plaintext);
    BOOST_REQUIRE(encrypted.has_value());
    BOOST_CHECK(encrypted->stored_bytes.size() <= cybou::ENCRYPTED_CHUNK_MAX_STORED_BYTES);
    BOOST_CHECK(IsPaddingBucket(encrypted->stored_bytes.size() - cybou::ENCRYPTED_CHUNK_HEADER_SIZE - 16));
    BOOST_CHECK(encrypted->id == cybou::ComputeChunkId(encrypted->stored_bytes));
    const auto decrypted = cybou::DecryptChunk(network, *key, encrypted->id, encrypted->stored_bytes);
    BOOST_REQUIRE(decrypted.has_value());
    BOOST_CHECK(*decrypted == plaintext);
}

BOOST_AUTO_TEST_CASE(encrypted_chunk_uses_fresh_salt_nonce_and_random_padding)
{
    const auto network = Network(0x22);
    const auto key = cybou::GenerateContentKey();
    BOOST_REQUIRE(key.has_value());
    const std::vector<unsigned char> plaintext(180 * 1024, 0x5a);

    const auto first = cybou::EncryptChunk(network, *key, plaintext);
    const auto second = cybou::EncryptChunk(network, *key, plaintext);
    BOOST_REQUIRE(first.has_value());
    BOOST_REQUIRE(second.has_value());
    BOOST_CHECK(first->stored_bytes != second->stored_bytes);
    BOOST_CHECK(first->id != second->id);
    BOOST_CHECK(IsPaddingBucket(first->stored_bytes.size() - cybou::ENCRYPTED_CHUNK_HEADER_SIZE - 16));
    BOOST_CHECK(IsPaddingBucket(second->stored_bytes.size() - cybou::ENCRYPTED_CHUNK_HEADER_SIZE - 16));
}

BOOST_AUTO_TEST_CASE(encrypted_chunk_rejects_wrong_network_key_and_address)
{
    const auto network = Network(0x31);
    const auto other_network = Network(0x32);
    const auto key = cybou::GenerateContentKey();
    BOOST_REQUIRE(key.has_value());
    const std::vector<unsigned char> plaintext{'p', 'r', 'i', 'v', 'a', 't', 'e'};
    const auto encrypted = cybou::EncryptChunk(network, *key, plaintext);
    BOOST_REQUIRE(encrypted.has_value());

    auto wrong_key = *key;
    wrong_key[0] ^= 0x80;
    BOOST_CHECK(!cybou::DecryptChunk(other_network, *key, encrypted->id, encrypted->stored_bytes));
    BOOST_CHECK(!cybou::DecryptChunk(network, wrong_key, encrypted->id, encrypted->stored_bytes));
    auto wrong_id = encrypted->id;
    wrong_id[0] ^= 1;
    BOOST_CHECK(!cybou::DecryptChunk(network, *key, wrong_id, encrypted->stored_bytes));
}

BOOST_AUTO_TEST_CASE(encrypted_chunk_rejects_tampering_magic_and_oversize)
{
    const auto network = Network(0x41);
    const auto key = cybou::GenerateContentKey();
    BOOST_REQUIRE(key.has_value());
    const std::vector<unsigned char> plaintext{'p', 'r', 'i', 'v', 'a', 't', 'e'};
    const auto encrypted = cybou::EncryptChunk(network, *key, plaintext);
    BOOST_REQUIRE(encrypted.has_value());

    auto tampered = encrypted->stored_bytes;
    tampered[cybou::ENCRYPTED_CHUNK_HEADER_SIZE + 2] ^= 0x40;
    BOOST_CHECK(!cybou::DecryptChunk(network, *key, cybou::ComputeChunkId(tampered), tampered));
    auto bad_magic = encrypted->stored_bytes;
    bad_magic[0] = 2;
    BOOST_CHECK(!cybou::DecryptChunk(network, *key, cybou::ComputeChunkId(bad_magic), bad_magic));
    std::vector<unsigned char> too_large(cybou::ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES + 1);
    BOOST_CHECK(!cybou::EncryptChunk(network, *key, too_large));
}

BOOST_AUTO_TEST_SUITE_END()
