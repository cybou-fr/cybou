// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_id.h>


#include <boost/test/unit_test.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace {

cybou::ChunkId FromHex(const std::string& hex)
{
    cybou::ChunkId result{};
    if (hex.size() != result.size() * 2) throw std::invalid_argument{"invalid ChunkId test vector"};
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<unsigned char>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    return result;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_chunk_id_tests)

BOOST_AUTO_TEST_CASE(chunk_id_matches_blake3_256_empty_input_known_answer)
{
    BOOST_CHECK(cybou::ComputeChunkId(std::span<const unsigned char>{}) == FromHex(
        "af1349b9f5f9a1a6a0404dea36dcc949"
        "9bcb25c9adc112b7cc9a93cae41f3262"));
}

BOOST_AUTO_TEST_CASE(chunk_id_matches_blake3_256_multichunk_official_vector)
{
    // BLAKE3-team/BLAKE3 test vector: input bytes repeat 0..250, input_len=1024.
    std::array<unsigned char, 1024> input{};
    for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<unsigned char>(i % 251);

    const auto expected = FromHex(
        "42214739f095a406f3fc83deb889744a"
        "c00df831c10daa55189b5d121c855af7");
    BOOST_CHECK(cybou::ComputeChunkId(input) == expected);
}

BOOST_AUTO_TEST_CASE(chunk_id_hashes_exact_stored_bytes)
{
    const std::array<unsigned char, 4> stored_a{0x01, 0x02, 0x03, 0x04};
    const std::array<unsigned char, 4> stored_b{0x01, 0x02, 0x03, 0x05};
    BOOST_CHECK(cybou::ComputeChunkId(stored_a) != cybou::ComputeChunkId(stored_b));
    BOOST_CHECK(cybou::ComputeChunkId(stored_a) == cybou::ComputeChunkId(stored_a));
}

BOOST_AUTO_TEST_SUITE_END()
