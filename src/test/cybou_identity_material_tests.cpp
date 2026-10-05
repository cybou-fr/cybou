// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#include <cybou/identity_material.h>
#include <cybou/identity_crypto.h>
#include <cybou/recovery_phrase.h>
#include <boost/test/unit_test.hpp>

#include <filesystem>

BOOST_AUTO_TEST_SUITE(cybou_identity_material_tests)

BOOST_AUTO_TEST_CASE(random_account_id_and_mnemonic_entropy_survive_encrypted_save)
{
    auto first = cybou::GenerateIdentityMaterial();
    auto second = cybou::GenerateIdentityMaterial();
    BOOST_REQUIRE(first && second);
    BOOST_CHECK(first->account_id != second->account_id);
    BOOST_CHECK(first->recovery_entropy != second->recovery_entropy);
    const auto words = cybou::EncodeRecoveryWords(first->recovery_entropy);
    BOOST_CHECK(cybou::DecodeRecoveryWords(words) == first->recovery_entropy);
    const auto authorization_before = cybou::DeriveIdentityPublicKey(first->recovery_entropy, cybou::IdentityKeyPurpose::AUTHORIZATION);
    const auto recovery_before = cybou::DeriveIdentityPublicKey(first->recovery_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto kem_seed_before = cybou::DeriveIdentityXWingSeed(first->recovery_entropy);
    BOOST_REQUIRE(authorization_before && recovery_before && kem_seed_before);
    BOOST_CHECK(cybou::ValidateXWingKeyPair(*kem_seed_before));

    const auto path = std::filesystem::temp_directory_path() / "cybou_identity_material_test.vault";
    std::filesystem::remove(path);
    BOOST_REQUIRE(cybou::SaveNewIdentityMaterial(path, "correct horse battery", *first));
    auto loaded = cybou::LoadIdentityMaterial(path, "correct horse battery");
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(loaded->account_id == first->account_id);
    BOOST_CHECK(loaded->recovery_entropy == first->recovery_entropy);
    BOOST_CHECK(cybou::DeriveIdentityPublicKey(loaded->recovery_entropy, cybou::IdentityKeyPurpose::AUTHORIZATION) == authorization_before);
    BOOST_CHECK(cybou::DeriveIdentityPublicKey(loaded->recovery_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT) == recovery_before);
    BOOST_CHECK(cybou::DeriveIdentityXWingSeed(loaded->recovery_entropy) == kem_seed_before);
    BOOST_CHECK(!cybou::LoadIdentityMaterial(path, "incorrect password"));
    BOOST_CHECK(!cybou::SaveNewIdentityMaterial(path, "correct horse battery", *second));
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_SUITE_END()
