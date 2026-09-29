// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_retention.h>

#include <cybou/chunk_blob_store.h>
#include <cybou/encrypted_chunk.h>

#include <boost/test/unit_test.hpp>

#include <set>
#include <string>
#include <vector>

namespace {

struct Blob {
    cybou::ChunkId id;
    std::vector<unsigned char> bytes;
};

Blob MakeBlob(cybou::ChunkBlobStore& store, unsigned char seed)
{
    Blob blob{.id = {}, .bytes = std::vector<unsigned char>(cybou::ENCRYPTED_CHUNK_MIN_STORED_BYTES, seed)};
    blob.bytes[0] ^= 0x5a;
    blob.id = cybou::ComputeChunkId(blob.bytes);
    BOOST_REQUIRE(store.Put(blob.id, blob.bytes) == cybou::ChunkBlobPutStatus::STORED);
    return blob;
}

cybou::RetentionKey Key(std::string_view holder, std::string_view reference)
{
    return {.holder = cybou::RetentionTag("test/holder", std::span{reinterpret_cast<const unsigned char*>(holder.data()), holder.size()}),
        .reference = cybou::RetentionTag("test/ref", std::span{reinterpret_cast<const unsigned char*>(reference.data()), reference.size()})};
}

constexpr std::uint64_t MINUTE{60'000};
constexpr std::uint64_t GRACE{10 * MINUTE};

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_chunk_retention_tests)

BOOST_AUTO_TEST_CASE(pins_are_per_reference_and_release_turns_them_into_cache)
{
    cybou::ChunkBlobStore blobs{{}, true};
    cybou::ChunkRetentionRegistry registry{{}, true, false};
    const auto a = MakeBlob(blobs, 1);
    const auto b = MakeBlob(blobs, 2);
    const auto job = Key("alice", "job-1");
    const auto offline = Key("alice", "offline-file");
    BOOST_REQUIRE(registry.Pin(job, std::vector{a.id, b.id}));
    BOOST_REQUIRE(registry.Pin(offline, std::vector{a.id}));
    BOOST_CHECK(registry.IsPinned(a.id) && registry.IsPinned(b.id));
    BOOST_CHECK_EQUAL(registry.Pinned(job).size(), 2U);

    // Releasing one reference keeps a chunk another reference still pins.
    BOOST_REQUIRE(registry.Release(job, 0));
    BOOST_CHECK(registry.IsPinned(a.id));
    BOOST_CHECK(!registry.IsPinned(b.id));
    BOOST_CHECK(registry.Pinned(job).empty());

    std::set<cybou::ChunkId> removed;
    const auto remove = [&](const cybou::ChunkId& id) { removed.insert(id); return blobs.Remove(id); };
    const auto result = registry.Collect(blobs, 0, GRACE + 1, GRACE, 100, remove);
    BOOST_CHECK_EQUAL(result.removed, 1U);
    BOOST_CHECK(removed == std::set{b.id});
    BOOST_CHECK(blobs.Has(a.id));
    BOOST_CHECK(!blobs.Has(b.id));
}

BOOST_AUTO_TEST_CASE(collection_is_lru_bounded_by_budget_grace_and_obligations)
{
    cybou::ChunkBlobStore blobs{{}, true};
    cybou::ChunkRetentionRegistry registry{{}, true, false};
    const auto size = cybou::ENCRYPTED_CHUNK_MIN_STORED_BYTES;
    const auto oldest = MakeBlob(blobs, 1);
    const auto older = MakeBlob(blobs, 2);
    const auto recent = MakeBlob(blobs, 3);
    const auto unknown = MakeBlob(blobs, 4); // never registered: never collected
    const auto provider = MakeBlob(blobs, 5);
    const std::uint64_t now{100 * MINUTE};
    BOOST_REQUIRE(registry.NoteCacheUse(oldest.id, now - 60 * MINUTE));
    BOOST_REQUIRE(registry.NoteCacheUse(older.id, now - 30 * MINUTE));
    BOOST_REQUIRE(registry.NoteCacheUse(recent.id, now - MINUTE));
    BOOST_REQUIRE(registry.NoteCacheUse(provider.id, now - 90 * MINUTE));

    // The provider-admitted blob refuses removal and stays.
    const auto remove = [&](const cybou::ChunkId& id) { return id != provider.id && blobs.Remove(id); };
    // Budget for two blobs: 4 evictable -> evict until it fits, oldest first.
    auto result = registry.Collect(blobs, 2 * size, now, GRACE, 100, remove);
    BOOST_CHECK_EQUAL(result.removed, 2U);
    BOOST_CHECK(blobs.Has(provider.id));
    BOOST_CHECK(!blobs.Has(oldest.id));
    BOOST_CHECK(!blobs.Has(older.id));
    BOOST_CHECK(blobs.Has(recent.id));
    BOOST_CHECK(blobs.Has(unknown.id));

    // Within the grace period nothing is evicted even over budget.
    result = registry.Collect(blobs, 0, now, GRACE, 100, remove);
    BOOST_CHECK_EQUAL(result.removed, 0U);
    BOOST_CHECK(blobs.Has(recent.id));

    // max_removals bounds one pass.
    result = registry.Collect(blobs, 0, now + GRACE, GRACE, 0, remove);
    BOOST_CHECK_EQUAL(result.removed, 0U);
    result = registry.Collect(blobs, 0, now + GRACE, GRACE, 100, remove);
    BOOST_CHECK_EQUAL(result.removed, 1U);
    BOOST_CHECK(!blobs.Has(recent.id));
    BOOST_CHECK(blobs.Has(unknown.id));
    BOOST_CHECK(blobs.Has(provider.id));

    // A pinned cache entry is never evicted.
    const auto pinned = MakeBlob(blobs, 6);
    BOOST_REQUIRE(registry.NoteCacheUse(pinned.id, 0));
    BOOST_REQUIRE(registry.Pin(Key("bob", "keep"), std::vector{pinned.id}));
    result = registry.Collect(blobs, 0, now + GRACE, GRACE, 100, remove);
    BOOST_CHECK(blobs.Has(pinned.id));
}

BOOST_AUTO_TEST_CASE(registry_survives_reopen)
{
    const auto dir = std::filesystem::temp_directory_path() / "cybou-retention-reopen-test";
    std::filesystem::remove_all(dir);
    cybou::ChunkBlobStore blobs{{}, true};
    const auto a = MakeBlob(blobs, 9);
    {
        cybou::ChunkRetentionRegistry registry{dir, false, false};
        BOOST_REQUIRE(registry.Pin(Key("alice", "job"), std::vector{a.id}));
    }
    {
        cybou::ChunkRetentionRegistry registry{dir, false, false};
        BOOST_CHECK(registry.IsPinned(a.id));
    }
    std::filesystem::remove_all(dir);
}

BOOST_AUTO_TEST_SUITE_END()
