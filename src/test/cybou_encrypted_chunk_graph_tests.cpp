// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/encrypted_chunk_graph.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <unordered_map>

namespace {

struct ChunkIdHash {
    std::size_t operator()(const cybou::ChunkId& id) const noexcept
    {
        std::size_t result{0};
        for (const auto byte : id) result = (result * 131) ^ byte;
        return result;
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_encrypted_chunk_graph_tests)

BOOST_AUTO_TEST_CASE(encrypted_chunk_graph_round_trips_empty_and_multileaf_payloads)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x53);
    for (const auto size : {std::size_t{0}, cybou::ENCRYPTED_GRAPH_LEAF_BYTES + 19}) {
        std::vector<unsigned char> plaintext(size);
        for (std::size_t i = 0; i < plaintext.size(); ++i) plaintext[i] = static_cast<unsigned char>((i * 17) & 0xff);

        const auto graph = cybou::BuildEncryptedChunkGraph(network, plaintext);
        BOOST_REQUIRE(graph.has_value());
        BOOST_CHECK(graph->plaintext_bytes == plaintext.size());
        BOOST_CHECK(graph->chunks.size() >= 1);

        std::unordered_map<cybou::ChunkId, std::vector<unsigned char>, ChunkIdHash> stored;
        for (const auto& chunk : graph->chunks) stored.emplace(chunk.id, chunk.stored_bytes);
        const auto recovered = cybou::FetchEncryptedChunkGraph(
            network, graph->content_key, graph->root_id,
            [&stored](const auto& id) -> std::optional<std::vector<unsigned char>> {
                const auto it = stored.find(id);
                if (it == stored.end()) return std::nullopt;
                return it->second;
            });
        BOOST_REQUIRE(recovered.has_value());
        BOOST_CHECK(*recovered == plaintext);
    }
}

BOOST_AUTO_TEST_CASE(encrypted_chunk_graph_rejects_wrong_key_and_missing_chunks)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x54);
    std::vector<unsigned char> plaintext(cybou::ENCRYPTED_GRAPH_LEAF_BYTES + 1, 0xa5);
    const auto graph = cybou::BuildEncryptedChunkGraph(network, plaintext);
    BOOST_REQUIRE(graph.has_value());

    std::unordered_map<cybou::ChunkId, std::vector<unsigned char>, ChunkIdHash> stored;
    for (const auto& chunk : graph->chunks) stored.emplace(chunk.id, chunk.stored_bytes);
    const auto lookup = [&stored](const auto& id) -> std::optional<std::vector<unsigned char>> {
        const auto it = stored.find(id);
        if (it == stored.end()) return std::nullopt;
        return it->second;
    };
    auto wrong_key = graph->content_key;
    wrong_key[0] ^= 1;
    BOOST_CHECK(!cybou::FetchEncryptedChunkGraph(network, wrong_key, graph->root_id, lookup));

    stored.erase(graph->chunks.front().id);
    BOOST_CHECK(!cybou::FetchEncryptedChunkGraph(network, graph->content_key, graph->root_id, lookup));
}

BOOST_AUTO_TEST_SUITE_END()
