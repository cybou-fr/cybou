// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_material.h>
#include <cybou/identity_crypto.h>
#include <cybou/recovery_phrase.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <filesystem>

BOOST_AUTO_TEST_SUITE(cybou_identity_material_tests)

BOOST_AUTO_TEST_CASE(random_account_id_and_recovery_entropy_survive_encrypted_save)
{
    auto first = cybou::GenerateIdentityMaterial();
    auto second = cybou::GenerateIdentityMaterial();
    BOOST_REQUIRE(first && second);
    BOOST_CHECK(first->account_id != second->account_id);
    BOOST_CHECK(first->account_id != first->recovery_entropy);
    BOOST_CHECK(first->device_secret != first->recovery_entropy);
    BOOST_CHECK(cybou::DecodeRecoveryWords(cybou::EncodeRecoveryWords(first->recovery_entropy)) == first->recovery_entropy);

    const auto path = std::filesystem::temp_directory_path() / "cybou_identity_material_v2_test.cybv2";
    std::filesystem::remove(path);
    BOOST_REQUIRE(cybou::SaveNewIdentityMaterial(path, "correct horse battery", *first));
    auto loaded = cybou::LoadIdentityMaterial(path, "correct horse battery");
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(loaded->account_id == first->account_id);
    BOOST_CHECK(loaded->recovery_entropy == first->recovery_entropy);
    BOOST_CHECK(loaded->device_secret == first->device_secret);
    const auto root_before = cybou::DeriveIdentityPublicKey(first->recovery_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto root_after = cybou::DeriveIdentityPublicKey(loaded->recovery_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(root_before && root_after);
    BOOST_CHECK(cybou::ComputeRecoveryKeyId(*root_before) == cybou::ComputeRecoveryKeyId(*root_after));
    BOOST_CHECK(!cybou::LoadIdentityMaterial(path, "incorrect password"));
    BOOST_CHECK(!cybou::SaveNewIdentityMaterial(path, "correct horse battery", *second));
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_SUITE_END()
