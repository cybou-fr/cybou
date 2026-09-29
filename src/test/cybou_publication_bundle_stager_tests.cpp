// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/publication_bundle_stager.h>
#include <cybou/canonical_cbor.h>
#include <test/cybou_test_setup.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

cybou::EncryptedTreeSource Source(const std::vector<unsigned char>& bytes)
{
    auto offset = std::make_shared<std::size_t>(0);
    return [&bytes, offset](const std::span<unsigned char> output) -> std::optional<std::size_t> {
        const auto count = std::min(output.size(), bytes.size() - *offset);
        std::copy_n(bytes.begin() + *offset, count, output.begin());
        *offset += count;
        return count;
    };
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(cybou_publication_bundle_stager_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(two_trees_share_one_durable_authorization_root)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x62);
    cybou::ChunkBlobStore blobs{m_data_dir / "chunks"};
    cybou::KVStore proof_db{cybou::KVStoreOptions{.path = m_data_dir / "proofs"}};
    const std::vector<unsigned char> child_data(400 * 1024, 0x41);
    const std::vector<unsigned char> empty;
    const auto metadata = cybou::EncodeCanonicalCbor(cybou::CborValue::ArrayValue({
        cybou::CborValue::Unsigned(1), cybou::CborValue::Text("private")
    }));

    cybou::StagedApplicationTree child;
    {
        cybou::PublicationBundleStager stager{blobs, proof_db, "mail-bundle-1", network};
        const auto staged = stager.StageTree(Source(child_data));
        BOOST_REQUIRE(staged);
        child = *staged;
        BOOST_CHECK(child.leaf_count > 1);
        BOOST_CHECK(blobs.Has(child.tree.root_chunk_id));
        BOOST_CHECK(!stager.GetProof(child.first_leaf));
    }
    {
        cybou::PublicationBundleStager restarted{blobs, proof_db, "mail-bundle-1", network};
        const auto main = restarted.StageTree(Source(empty), metadata);
        BOOST_REQUIRE(main);
        BOOST_CHECK_EQUAL(main->first_leaf, child.leaf_count);
        BOOST_CHECK_EQUAL(main->leaf_count, 1U);
        const auto publication = restarted.Finish(*main);
        BOOST_REQUIRE(publication);
        BOOST_CHECK_EQUAL(publication->chunk_count, child.leaf_count + main->leaf_count);
        BOOST_CHECK(publication->root_chunk_id == main->tree.root_chunk_id);
        BOOST_CHECK(publication->chunk_authorization_root != main->tree.chunk_authorization_root);
        for (std::uint32_t index{0}; index < publication->chunk_count; ++index) {
            const auto id = restarted.GetLeafId(index);
            const auto proof = restarted.GetProof(index);
            BOOST_REQUIRE(id && proof);
            BOOST_CHECK(blobs.Has(*id));
            BOOST_CHECK(cybou::VerifyChunkAuthorizationPath(publication->chunk_authorization_root,
                *id, proof->leaf_index, publication->chunk_count, proof->siblings));
        }
    }
}

BOOST_AUTO_TEST_CASE(interrupted_tree_requires_discard_before_reuse)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x63);
    cybou::ChunkBlobStore blobs{m_data_dir / "chunks"};
    cybou::KVStore proof_db{cybou::KVStoreOptions{.path = m_data_dir / "proofs"}};
    const std::vector<unsigned char> empty;
    {
        cybou::PublicationBundleStager stager{blobs, proof_db, "interrupted", network};
        BOOST_CHECK(!stager.StageTree([](std::span<unsigned char>) -> std::optional<std::size_t> {
            return std::nullopt;
        }));
        BOOST_CHECK(!stager.StageTree(Source(empty)));
    }
    {
        cybou::PublicationBundleStager reopened{blobs, proof_db, "interrupted", network};
        BOOST_CHECK(!reopened.StageTree(Source(empty)));
        BOOST_CHECK(reopened.Discard());
    }
    cybou::PublicationBundleStager fresh{blobs, proof_db, "next-attempt", network};
    const auto main = fresh.StageTree(Source(empty));
    BOOST_REQUIRE(main);
    BOOST_CHECK(fresh.Finish(*main).has_value());
}

BOOST_AUTO_TEST_CASE(index_rejects_another_network_and_wrong_main_key)
{
    std::array<unsigned char, 32> network{};
    network.fill(0x64);
    cybou::ChunkBlobStore blobs{m_data_dir / "chunks"};
    cybou::KVStore proof_db{cybou::KVStoreOptions{.path = m_data_dir / "proofs"}};
    const std::vector<unsigned char> empty;
    {
        cybou::PublicationBundleStager stager{blobs, proof_db, "bound", network};
        const auto main = stager.StageTree(Source(empty));
        BOOST_REQUIRE(main);
        auto wrong = *main;
        wrong.tree.content_key[0] ^= 1;
        BOOST_CHECK(!stager.Finish(wrong));
        BOOST_CHECK(stager.Finish(*main).has_value());
    }
    network[0] ^= 1;
    BOOST_CHECK_THROW((cybou::PublicationBundleStager{blobs, proof_db, "bound", network}),
        std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
