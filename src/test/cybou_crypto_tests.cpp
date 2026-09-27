// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/crypto/sha256.h>

#include <crypto/sha256.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>\n#include <array>
#include <cstddef>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_crypto_tests)

BOOST_AUTO_TEST_CASE(openssl_sha256_matches_standard_vectors_and_legacy_output)
{
    const auto empty_expected = ParseHex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const auto abc_expected = ParseHex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> empty_digest{};
    cybou::crypto::Sha256{}.Finalize(empty_digest.data());
    BOOST_CHECK_EQUAL_COLLECTIONS(empty_digest.begin(), empty_digest.end(), empty_expected.begin(), empty_expected.end());

    constexpr std::array<unsigned char, 3> abc{'a', 'b', 'c'};
    std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> abc_digest{};
    cybou::crypto::Sha256{}.Write(abc.data(), abc.size()).Finalize(abc_digest.data());
    BOOST_CHECK_EQUAL_COLLECTIONS(abc_digest.begin(), abc_digest.end(), abc_expected.begin(), abc_expected.end());

    constexpr std::array<std::size_t, 8> sizes{1, 55, 56, 63, 64, 65, 1024, 8193};\n    for (const std::size_t size : sizes) {
        std::vector<unsigned char> input(size);
        for (std::size_t i = 0; i < input.size(); ++i) {
            input[i] = static_cast<unsigned char>((i * 37 + 11) % 251);
        }

        std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> legacy_digest{};
        CSHA256 legacy;
        if (!input.empty()) legacy.Write(input.data(), input.size());
        legacy.Finalize(legacy_digest.data());

        std::array<unsigned char, cybou::crypto::Sha256::OUTPUT_SIZE> evp_digest{};
        cybou::crypto::Sha256 evp;
        for (std::size_t offset = 0; offset < input.size();) {
            const std::size_t chunk_size = std::min<std::size_t>(17, input.size() - offset);
            evp.Write(input.data() + offset, chunk_size);
            offset += chunk_size;
        }
        evp.Finalize(evp_digest.data());
        BOOST_CHECK_EQUAL_COLLECTIONS(evp_digest.begin(), evp_digest.end(), legacy_digest.begin(), legacy_digest.end());
    }
}

BOOST_AUTO_TEST_SUITE_END()
