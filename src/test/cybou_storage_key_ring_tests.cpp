// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_key_ring.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <atomic>
#include <filesystem>
#include <thread>

BOOST_AUTO_TEST_SUITE(cybou_storage_key_ring_tests)

BOOST_AUTO_TEST_CASE(account_bound_key_epochs_survive_rotation_and_restart)
{
    const std::array<unsigned char, 32> account_bytes{
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto account = cybou::AccountId::FromBytes(account_bytes);
    BOOST_REQUIRE(account);
    const auto path = std::filesystem::temp_directory_path() / "cybou_storage_key_ring_test.cybv2";
    std::filesystem::remove(path);
    auto lock_path = path;
    lock_path += ".lock";
    std::filesystem::remove(lock_path);

    auto ring = cybou::StorageKeyRing::Create(*account);
    BOOST_REQUIRE(ring);
    BOOST_CHECK_EQUAL(ring->CurrentEpoch(), 0U);
    BOOST_REQUIRE(ring->SaveNewToFile(path, "correct horse battery"));

    cybou::StorageMasterKey epoch_zero{};
    BOOST_REQUIRE(ring->CopyMasterKey(0, epoch_zero));
    cybou::StorageMasterKey missing_epoch{};
    BOOST_CHECK(!ring->CopyMasterKey(1, missing_epoch));
    BOOST_REQUIRE(ring->RotateAndSave(path, "correct horse battery"));
    BOOST_CHECK_EQUAL(ring->CurrentEpoch(), 1U);
    cybou::StorageMasterKey epoch_one{};
    BOOST_REQUIRE(ring->CopyMasterKey(1, epoch_one));
    BOOST_CHECK(epoch_zero != epoch_one);

    auto reopened = cybou::StorageKeyRing::LoadFromFile(path, "correct horse battery");
    BOOST_REQUIRE(reopened);
    BOOST_CHECK(reopened->BoundAccount() == *account);
    BOOST_CHECK_EQUAL(reopened->CurrentEpoch(), 1U);
    cybou::StorageMasterKey reopened_zero{};
    cybou::StorageMasterKey reopened_one{};
    BOOST_REQUIRE(reopened->CopyMasterKey(0, reopened_zero));
    BOOST_REQUIRE(reopened->CopyMasterKey(1, reopened_one));
    BOOST_CHECK(reopened_zero == epoch_zero);
    BOOST_CHECK(reopened_one == epoch_one);

    const std::array<unsigned char, 32> other_account_bytes{
        2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto other_account = cybou::AccountId::FromBytes(other_account_bytes);
    BOOST_REQUIRE(other_account);
    BOOST_CHECK(reopened->BoundAccount() != *other_account);
    BOOST_CHECK(!cybou::StorageKeyRing::LoadFromFile(path, "incorrect password"));

    reopened_zero.fill(0);
    reopened_one.fill(0);
    epoch_zero.fill(0);
    epoch_one.fill(0);
    std::filesystem::remove(path);
    std::filesystem::remove(lock_path);
}

BOOST_AUTO_TEST_CASE(rotation_fails_closed_when_saved_keyring_changed)
{
    const std::array<unsigned char, 32> account_bytes{
        3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto account = cybou::AccountId::FromBytes(account_bytes);
    BOOST_REQUIRE(account);
    const auto path = std::filesystem::temp_directory_path() / "cybou_storage_key_ring_conflict_test.cybv2";
    std::filesystem::remove(path);
    auto first = cybou::StorageKeyRing::Create(*account);
    auto second = cybou::StorageKeyRing::Create(*account);
    BOOST_REQUIRE(first && second);
    BOOST_REQUIRE(first->SaveNewToFile(path, "correct horse battery"));
    const auto other_path = std::filesystem::temp_directory_path() / "cybou_storage_key_ring_other_test.cybv2";
    std::filesystem::remove(other_path);
    BOOST_REQUIRE(second->SaveNewToFile(other_path, "correct horse battery"));
    BOOST_CHECK(!first->RotateAndSave(other_path, "correct horse battery"));
    BOOST_CHECK_EQUAL(first->CurrentEpoch(), 0U);
    std::filesystem::remove(path);
    std::filesystem::remove(other_path);
    auto other_lock = other_path;
    other_lock += ".lock";
    std::filesystem::remove(other_lock);
}

BOOST_AUTO_TEST_CASE(concurrent_stale_keyring_rotations_commit_at_most_once)
{
    const std::array<unsigned char, 32> account_bytes{
        4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto account = cybou::AccountId::FromBytes(account_bytes);
    BOOST_REQUIRE(account);
    const auto path = std::filesystem::temp_directory_path() / "cybou_storage_key_ring_concurrent_test.cybv2";
    std::filesystem::remove(path);
    auto original = cybou::StorageKeyRing::Create(*account);
    BOOST_REQUIRE(original);
    BOOST_REQUIRE(original->SaveNewToFile(path, "correct horse battery"));
    auto first = cybou::StorageKeyRing::LoadFromFile(path, "correct horse battery");
    auto second = cybou::StorageKeyRing::LoadFromFile(path, "correct horse battery");
    BOOST_REQUIRE(first && second);

    std::atomic<unsigned> successful_rotations{0};
    std::thread a([&] { if (first->RotateAndSave(path, "correct horse battery")) ++successful_rotations; });
    std::thread b([&] { if (second->RotateAndSave(path, "correct horse battery")) ++successful_rotations; });
    a.join();
    b.join();

    BOOST_CHECK_EQUAL(successful_rotations.load(), 1U);
    const auto reopened = cybou::StorageKeyRing::LoadFromFile(path, "correct horse battery");
    BOOST_REQUIRE(reopened);
    BOOST_CHECK_EQUAL(reopened->CurrentEpoch(), 1U);
    std::filesystem::remove(path);
    auto lock_path = path;
    lock_path += ".lock";
    std::filesystem::remove(lock_path);
}

BOOST_AUTO_TEST_SUITE_END()
