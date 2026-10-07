// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#include <cybou/state_store.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>

BOOST_AUTO_TEST_SUITE(cybou_kv_store_tests)

BOOST_AUTO_TEST_CASE(local_record_encoding_preserves_existing_database_bytes)
{
    const auto encoded_string = cybou::detail::SerializeLocalRecord(std::string{"key"});
    const std::array<unsigned char, 4> expected_string{3, 'k', 'e', 'y'};
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_string.begin(), encoded_string.end(),
        expected_string.begin(), expected_string.end());

    const auto encoded_integer = cybou::detail::SerializeLocalRecord(uint64_t{0x0102030405060708});
    const std::array<unsigned char, 8> expected_integer{8, 7, 6, 5, 4, 3, 2, 1};
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_integer.begin(), encoded_integer.end(),
        expected_integer.begin(), expected_integer.end());

    const std::vector<unsigned char> value{0xaa, 0xbb};
    const auto encoded_vector = cybou::detail::SerializeLocalRecord(value);
    const std::array<unsigned char, 3> expected_vector{2, 0xaa, 0xbb};
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_vector.begin(), encoded_vector.end(),
        expected_vector.begin(), expected_vector.end());

    const std::vector<unsigned char> compact_253(253, 0xaa);
    const auto encoded_253 = cybou::detail::SerializeLocalRecord(compact_253);
    BOOST_CHECK_EQUAL(encoded_253[0], 253);
    BOOST_CHECK_EQUAL(encoded_253[1], 253);
    BOOST_CHECK_EQUAL(encoded_253[2], 0);

    const std::vector<unsigned char> compact_65536(65536, 0xbb);
    const auto encoded_65536 = cybou::detail::SerializeLocalRecord(compact_65536);
    BOOST_CHECK_EQUAL(encoded_65536[0], 254);
    BOOST_CHECK_EQUAL(encoded_65536[1], 0);
    BOOST_CHECK_EQUAL(encoded_65536[2], 0);
    BOOST_CHECK_EQUAL(encoded_65536[3], 1);
    BOOST_CHECK_EQUAL(encoded_65536[4], 0);

    std::string decoded;
    BOOST_CHECK(cybou::detail::DeserializeLocalRecord(std::span{encoded_string}, decoded));
    BOOST_CHECK_EQUAL(decoded, "key");
    const std::array<unsigned char, 3> noncanonical_string_size{253, 1, 0};
    BOOST_CHECK(!cybou::detail::DeserializeLocalRecord(std::span{noncanonical_string_size}, decoded));

    cybou::FinalizedHead head{};
    for (size_t i{0}; i < cybou::Hash256::size(); ++i) head.block_id.begin()[i] = static_cast<unsigned char>(i);
    head.height = 0x0102030405060708;
    const auto encoded_head = cybou::detail::SerializeLocalRecord(head);
    BOOST_REQUIRE_EQUAL(encoded_head.size(), 40);
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_head.begin(), encoded_head.begin() + 32,
        head.block_id.begin(), head.block_id.end());
    const std::array<unsigned char, 8> expected_height{8, 7, 6, 5, 4, 3, 2, 1};
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_head.begin() + 32, encoded_head.end(),
        expected_height.begin(), expected_height.end());

    cybou::FinalizedHead decoded_head{};
    BOOST_CHECK(cybou::detail::DeserializeLocalRecord(std::span{encoded_head}, decoded_head));
    BOOST_CHECK(decoded_head.block_id == head.block_id);
    BOOST_CHECK_EQUAL(decoded_head.height, head.height);
}

BOOST_AUTO_TEST_CASE(disk_restart_preserves_records_and_missing_current_preserves_evidence)
{
    const auto path=std::filesystem::temp_directory_path() /
        ("cybou-kv-reopen-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    BOOST_REQUIRE(std::filesystem::create_directory(path));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path,ec); } } cleanup{path};
    std::filesystem::create_directory(path/"geo"); // A new node's non-DB files are allowed.
    {
        cybou::KVStore db{{.path=path}};
        db.Write(std::string{"cybou/network-id"},cybou::Hash256{uint8_t{1}},true);
        const std::vector<unsigned char> payload(1024*1024,0x5a);
        for (unsigned i=0;i<12;++i) db.Write("payload-"+std::to_string(i),payload,true);
    }
    BOOST_REQUIRE(std::filesystem::exists(path/"CURRENT"));
    {
        cybou::KVStore db{{.path=path}};
        cybou::Hash256 binding;
        BOOST_REQUIRE(db.Read(std::string{"cybou/network-id"},binding));
        BOOST_CHECK(binding==cybou::Hash256{uint8_t{1}});
        std::vector<unsigned char> payload;
        BOOST_REQUIRE(db.Read(std::string{"payload-0"},payload));
        BOOST_CHECK_EQUAL(payload.size(),1024*1024);
    }
    std::filesystem::rename(path/"CURRENT",path/"CURRENT.saved");
    std::map<std::string,uintmax_t> before,after;
    for (const auto& entry : std::filesystem::directory_iterator{path}) if(entry.is_regular_file())
        before[entry.path().filename().string()]=entry.file_size();
    BOOST_CHECK_THROW(cybou::KVStore(cybou::KVStoreOptions{.path=path}),std::runtime_error);
    for (const auto& entry : std::filesystem::directory_iterator{path}) if(entry.is_regular_file())
        after[entry.path().filename().string()]=entry.file_size();
    BOOST_CHECK(before==after); // No recovery, table deletion or replacement CURRENT.
}

BOOST_AUTO_TEST_SUITE_END()
