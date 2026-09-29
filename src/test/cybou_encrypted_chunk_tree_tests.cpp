// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/encrypted_chunk_tree.h>
#include <cybou/canonical_cbor.h>


#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace {

struct ChunkIdHash {
    std::size_t operator()(const cybou::ChunkId& id) const noexcept
    {
        std::size_t result{0};
        for (const auto byte : id) result = (result * 131) ^ byte;
        return result;
    }
};

cybou::EncryptedTreeSource Source(const std::vector<unsigned char>& bytes)
{
    auto offset = std::make_shared<std::size_t>(0);
    return [&bytes, offset](std::span<unsigned char> output) -> std::optional<std::size_t> {
        const auto count = std::min(output.size(), bytes.size() - *offset);
        std::copy_n(bytes.begin() + *offset, count, output.begin());
        *offset += count;
        return count;
    };
}

cybou::EncryptedTreeVisit VisitSet()
{
    auto ids = std::make_shared<std::unordered_set<cybou::ChunkId, ChunkIdHash>>();
    return [ids](const cybou::ChunkId& id) { return ids->insert(id).second; };
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_encrypted_chunk_tree_tests)

BOOST_AUTO_TEST_CASE(encrypted_chunk_tree_streams_empty_and_multichunk_payloads)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x53);
    for (const auto size : {std::size_t{0}, std::size_t{900 * 1024 + 19}}) {
        std::vector<unsigned char> plaintext(size);
        for (std::size_t i = 0; i < plaintext.size(); ++i) plaintext[i] = static_cast<unsigned char>((i * 17) & 0xff);

        std::unordered_map<cybou::ChunkId, std::vector<unsigned char>, ChunkIdHash> stored;
        cybou::ChunkAuthorizationAccumulator authorization;
        const auto app_metadata = cybou::EncodeCanonicalCbor(cybou::CborValue::MapValue({
            {cybou::CborValue::Text("private_schema"), cybou::CborValue::Text("files-v1")},
        }));
        const auto tree = cybou::BuildEncryptedChunkTree(network, Source(plaintext),
            [&stored, &authorization](const auto leaf_index, const auto& chunk) {
                if (leaf_index != stored.size() || !stored.emplace(chunk.id, chunk.stored_bytes).second) return false;
                return authorization.Add({chunk.id});
            }, app_metadata);
        BOOST_REQUIRE(tree.has_value());
        const auto authorization_summary = authorization.Finish();
        BOOST_REQUIRE(authorization_summary.has_value());
        BOOST_CHECK(tree->chunk_authorization_root == authorization_summary->root);
        BOOST_CHECK(tree->chunk_count == authorization_summary->chunk_count);
        BOOST_CHECK(tree->plaintext_bytes == plaintext.size());
        BOOST_CHECK(tree->chunk_count == stored.size());
        BOOST_CHECK(tree->chunk_count >= 1); // ROOT is always staged, including empty objects
        if (size == 0) BOOST_CHECK(tree->chunk_count == 1);
        const auto root_stored = stored.find(tree->root_chunk_id);
        BOOST_REQUIRE(root_stored != stored.end());
        const auto root_bytes = cybou::DecryptChunk(network, tree->content_key,
            tree->root_chunk_id, root_stored->second);
        BOOST_REQUIRE(root_bytes.has_value());
        const auto root_value = cybou::DecodeCanonicalCbor(*root_bytes);
        const auto* root_fields = std::get_if<cybou::CborValue::Map>(&root_value.value);
        BOOST_REQUIRE(root_fields != nullptr);
        const auto* root_children = std::get_if<cybou::CborValue::Array>(&(*root_fields)[3].second.value);
        BOOST_REQUIRE(root_children != nullptr);
        if (size == 0) {
            BOOST_CHECK(root_children->empty());
        } else {
            BOOST_REQUIRE(!root_children->empty());
            const auto* first_child = std::get_if<cybou::CborValue::ByteString>(&root_children->front().value);
            BOOST_REQUIRE(first_child != nullptr && first_child->size() == cybou::ChunkId{}.size());
            const auto* root_child_kind = std::get_if<std::uint64_t>(&(*root_fields)[2].second.value);
            BOOST_REQUIRE(root_child_kind != nullptr);
            BOOST_CHECK(*root_child_kind == 2); // small object: ROOT points directly to DATA
        }

        std::vector<unsigned char> recovered;
        const auto byte_count = cybou::FetchEncryptedChunkTree(
            network, tree->content_key, tree->root_chunk_id,
            [&stored](const auto& id) -> std::optional<std::vector<unsigned char>> {
                const auto it = stored.find(id);
                if (it == stored.end()) return std::nullopt;
                return it->second;
            },
            [&app_metadata](const auto metadata) { return std::equal(metadata.begin(), metadata.end(), app_metadata.begin(), app_metadata.end()); },
            VisitSet(),
            [&recovered](const auto bytes) { recovered.insert(recovered.end(), bytes.begin(), bytes.end()); return true; },
            plaintext.size());
        BOOST_REQUIRE(byte_count.has_value());
        BOOST_CHECK(*byte_count == plaintext.size());
        BOOST_CHECK(recovered == plaintext);
    }
}

BOOST_AUTO_TEST_CASE(encrypted_chunk_tree_rejects_wrong_key_missing_chunks_and_sink_overflow)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x54);
    std::vector<unsigned char> plaintext(800 * 1024, 0xa5);
    std::unordered_map<cybou::ChunkId, std::vector<unsigned char>, ChunkIdHash> stored;
    const auto tree = cybou::BuildEncryptedChunkTree(network, Source(plaintext),
        [&stored](const auto leaf_index, const auto& chunk) {
            return leaf_index == stored.size() && stored.emplace(chunk.id, chunk.stored_bytes).second;
        });
    BOOST_REQUIRE(tree.has_value());
    const auto lookup = [&stored](const auto& id) -> std::optional<std::vector<unsigned char>> {
        const auto it = stored.find(id);
        if (it == stored.end()) return std::nullopt;
        return it->second;
    };
    const auto discard = [](const auto) { return true; };
    auto wrong_key = tree->content_key;
    wrong_key[0] ^= 1;
    BOOST_CHECK(!cybou::FetchEncryptedChunkTree(network, wrong_key, tree->root_chunk_id, lookup, [](const auto) { return true; }, VisitSet(), discard, plaintext.size()));
    BOOST_CHECK(!cybou::FetchEncryptedChunkTree(network, tree->content_key, tree->root_chunk_id, lookup, [](const auto) { return true; }, VisitSet(), discard, plaintext.size() - 1));

    const auto first_chunk = stored.begin()->first;
    stored.erase(first_chunk);
    BOOST_CHECK(!cybou::FetchEncryptedChunkTree(network, tree->content_key, tree->root_chunk_id, lookup, [](const auto) { return true; }, VisitSet(), discard, plaintext.size()));
}

BOOST_AUTO_TEST_CASE(encrypted_chunk_tree_adds_index_after_root_fanout_is_exceeded)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x56);
    std::vector<unsigned char> plaintext(
        cybou::ENCRYPTED_TREE_MAX_CHILDREN * cybou::ENCRYPTED_TREE_DATA_MAX_BYTES + 1, 0x37);
    std::unordered_map<cybou::ChunkId, std::vector<unsigned char>, ChunkIdHash> stored;
    const auto tree = cybou::BuildEncryptedChunkTree(network, Source(plaintext),
        [&stored](const auto leaf_index, const auto& chunk) {
            return leaf_index == stored.size() && stored.emplace(chunk.id, chunk.stored_bytes).second;
        });
    BOOST_REQUIRE(tree.has_value());
    const auto root = stored.find(tree->root_chunk_id);
    BOOST_REQUIRE(root != stored.end());
    const auto root_bytes = cybou::DecryptChunk(network, tree->content_key, tree->root_chunk_id, root->second);
    BOOST_REQUIRE(root_bytes.has_value());
    const auto root_value = cybou::DecodeCanonicalCbor(*root_bytes);
    const auto* fields = std::get_if<cybou::CborValue::Map>(&root_value.value);
    BOOST_REQUIRE(fields != nullptr);
    const auto* children = std::get_if<cybou::CborValue::Array>(&(*fields)[3].second.value);
    BOOST_REQUIRE(children != nullptr && !children->empty());
    const auto* child = std::get_if<cybou::CborValue::ByteString>(&children->front().value);
    BOOST_REQUIRE(child != nullptr && child->size() == cybou::ChunkId{}.size());
    const auto* kind = std::get_if<std::uint64_t>(&(*fields)[2].second.value);
    BOOST_REQUIRE(kind != nullptr);
    BOOST_CHECK(*kind == 1); // ROOT now points to INDEX; DATA remains beneath it.
    BOOST_CHECK(tree->chunk_count > cybou::ENCRYPTED_TREE_MAX_CHILDREN);
}

BOOST_AUTO_TEST_CASE(encrypted_chunk_tree_stager_failure_aborts_build)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x55);
    std::vector<unsigned char> plaintext(1024, 0x42);
    const auto tree = cybou::BuildEncryptedChunkTree(network, Source(plaintext),
        [](const auto, const auto&) { return false; });
    BOOST_CHECK(!tree);
}

BOOST_AUTO_TEST_SUITE_END()
