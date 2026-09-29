// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/crypto/sha256.h>
#include <cybou/crypto/hkdf_sha256.h>
#include <cybou/crypto/chacha20_poly1305.h>
#include <test/cybou_test_helpers.h>


#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_crypto_tests)

BOOST_AUTO_TEST_CASE(openssl_sha256_matches_standard_vectors_and_streaming)
{
    const auto empty_expected = cybou::test::ParseHex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const auto abc_expected = cybou::test::ParseHex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> empty_digest{};
    cybou::crypto::Sha256{}.Finalize(empty_digest.data());
    BOOST_CHECK_EQUAL_COLLECTIONS(empty_digest.begin(), empty_digest.end(), empty_expected.begin(), empty_expected.end());

    constexpr std::array<unsigned char, 3> abc{'a', 'b', 'c'};
    std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> abc_digest{};
    cybou::crypto::Sha256{}.Write(abc.data(), abc.size()).Finalize(abc_digest.data());
    BOOST_CHECK_EQUAL_COLLECTIONS(abc_digest.begin(), abc_digest.end(), abc_expected.begin(), abc_expected.end());
    std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> multipart_digest{};
    BOOST_REQUIRE(cybou::crypto::ComputeSha256({
        cybou::crypto::Sha256Bytes("a"), std::span<const unsigned char>{abc.data() + 1, 1},
        std::span<const unsigned char>{abc.data() + 2, 1},
    }, multipart_digest.data()));
    BOOST_CHECK_EQUAL_COLLECTIONS(multipart_digest.begin(), multipart_digest.end(), abc_digest.begin(), abc_digest.end());
    BOOST_CHECK(!cybou::crypto::ComputeSha256({}, nullptr));

    constexpr std::array<std::size_t, 8> sizes{1, 55, 56, 63, 64, 65, 1024, 8193};
    for (const std::size_t size : sizes) {
        std::vector<unsigned char> input(size);
        for (std::size_t i = 0; i < input.size(); ++i) {
            input[i] = static_cast<unsigned char>((i * 37 + 11) % 251);
        }

        std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> single_digest{};
        cybou::crypto::Sha256{}.Write(input.data(), input.size()).Finalize(single_digest.data());
        std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> evp_digest{};
        cybou::crypto::Sha256 evp;
        for (std::size_t offset = 0; offset < input.size();) {
            const std::size_t chunk_size = std::min<std::size_t>(17, input.size() - offset);
            evp.Write(input.data() + offset, chunk_size);
            offset += chunk_size;
        }
        evp.Finalize(evp_digest.data());
        BOOST_CHECK_EQUAL_COLLECTIONS(evp_digest.begin(), evp_digest.end(), single_digest.begin(), single_digest.end());
    }
}

BOOST_AUTO_TEST_CASE(hkdf_sha256_matches_rfc5869_vectors)
{
    const auto ikm = cybou::test::ParseHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
    const auto salt = cybou::test::ParseHex("000102030405060708090a0b0c");
    const auto info = cybou::test::ParseHex("f0f1f2f3f4f5f6f7f8f9");
    const auto expected = cybou::test::ParseHex(
        "3cb25f25faacd57a90434f64d0362f2a"
        "2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
        "34007208d5b887185865");
    std::array<unsigned char, 42> output{};
    BOOST_REQUIRE(cybou::crypto::HkdfSha256(ikm, salt, info, output));
    BOOST_CHECK_EQUAL_COLLECTIONS(output.begin(), output.end(), expected.begin(), expected.end());

    const auto empty = cybou::test::ParseHex("");
    const auto empty_salt_expected = cybou::test::ParseHex(
        "8da4e775a563c18f715f802a063c5a31"
        "b8a11f5c5ee1879ec3454e5f3c738d2d"
        "9d201395faa4b61a96c8");
    BOOST_REQUIRE(cybou::crypto::HkdfSha256(ikm, empty, empty, output));
    BOOST_CHECK_EQUAL_COLLECTIONS(output.begin(), output.end(), empty_salt_expected.begin(), empty_salt_expected.end());

    std::array<unsigned char, 255 * 32 + 1> oversized{};
    BOOST_CHECK(!cybou::crypto::HkdfSha256(ikm, salt, info, oversized));
}

BOOST_AUTO_TEST_CASE(chacha20_poly1305_matches_rfc8439)
{
    const auto key = cybou::test::ParseHex(
        "808182838485868788898a8b8c8d8e8f"
        "909192939495969798999a9b9c9d9e9f");
    const auto nonce = cybou::test::ParseHex("070000004041424344454647");
    const auto aad = cybou::test::ParseHex("50515253c0c1c2c3c4c5c6c7");
    const std::string_view plaintext_text{
        "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it."};
    const auto plaintext = std::span<const unsigned char>{
        reinterpret_cast<const unsigned char*>(plaintext_text.data()), plaintext_text.size()};
    const auto expected = cybou::test::ParseHex(
        "d31a8d34648e60db7b86afbc53ef7ec2"
        "a4aded51296e08fea9e2b5a736ee62d6"
        "3dbea45e8ca9671282fafb69da92728b"
        "1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58"
        "fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116"
        "1ae10b594f09e26a7e902ecbd0600691");

    std::vector<unsigned char> evp_ciphertext(expected.size());
    BOOST_REQUIRE(cybou::crypto::ChaCha20Poly1305Encrypt(
        std::span<const unsigned char, 32>{key.data(), key.size()},
        std::span<const unsigned char, 12>{nonce.data(), nonce.size()},
        aad, plaintext, evp_ciphertext));
    BOOST_CHECK_EQUAL_COLLECTIONS(evp_ciphertext.begin(), evp_ciphertext.end(), expected.begin(), expected.end());

    std::vector<unsigned char> decrypted(plaintext.size());
    BOOST_REQUIRE(cybou::crypto::ChaCha20Poly1305Decrypt(
        std::span<const unsigned char, 32>{key.data(), key.size()},
        std::span<const unsigned char, 12>{nonce.data(), nonce.size()},
        aad, evp_ciphertext, decrypted));
    BOOST_CHECK_EQUAL_COLLECTIONS(decrypted.begin(), decrypted.end(), plaintext.begin(), plaintext.end());

    auto tampered = evp_ciphertext;
    tampered.back() ^= 0x01;
    std::fill(decrypted.begin(), decrypted.end(), 0xa5);
    BOOST_CHECK(!cybou::crypto::ChaCha20Poly1305Decrypt(
        std::span<const unsigned char, 32>{key.data(), key.size()},
        std::span<const unsigned char, 12>{nonce.data(), nonce.size()},
        aad, tampered, decrypted));
    BOOST_CHECK(std::all_of(decrypted.begin(), decrypted.end(), [](const unsigned char byte) { return byte == 0; }));

    auto wrong_aad = aad;
    wrong_aad[0] ^= 0x01;
    std::fill(decrypted.begin(), decrypted.end(), 0xa5);
    BOOST_CHECK(!cybou::crypto::ChaCha20Poly1305Decrypt(
        std::span<const unsigned char, 32>{key.data(), key.size()},
        std::span<const unsigned char, 12>{nonce.data(), nonce.size()},
        wrong_aad, evp_ciphertext, decrypted));
    BOOST_CHECK(std::all_of(decrypted.begin(), decrypted.end(), [](const unsigned char byte) { return byte == 0; }));

    std::vector<unsigned char> wrong_size(evp_ciphertext.size() - 1, 0xa5);
    BOOST_CHECK(!cybou::crypto::ChaCha20Poly1305Encrypt(
        std::span<const unsigned char, 32>{key.data(), key.size()},
        std::span<const unsigned char, 12>{nonce.data(), nonce.size()},
        aad, plaintext, wrong_size));
    BOOST_CHECK(std::all_of(wrong_size.begin(), wrong_size.end(), [](const unsigned char byte) { return byte == 0; }));

    std::vector<unsigned char> short_ciphertext(cybou::crypto::CHACHA20_POLY1305_TAG_SIZE - 1);
    std::vector<unsigned char> empty_plaintext;
    BOOST_CHECK(!cybou::crypto::ChaCha20Poly1305Decrypt(
        std::span<const unsigned char, 32>{key.data(), key.size()},
        std::span<const unsigned char, 12>{nonce.data(), nonce.size()},
        aad, short_ciphertext, empty_plaintext));
}

BOOST_AUTO_TEST_SUITE_END()
