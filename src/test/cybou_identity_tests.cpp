// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>
#include <cybou/identity_authorization.h>
#include <cybou/keystore.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

BOOST_AUTO_TEST_SUITE(cybou_identity_tests)

BOOST_AUTO_TEST_CASE(operator_key_domains_are_distinct)
{
    using cybou::OperatorKeyDomain;
    constexpr std::array domains{
        OperatorKeyDomain::AUTHORITY,
        OperatorKeyDomain::VALIDATOR,
        OperatorKeyDomain::RELEASE_SIGNING,
        OperatorKeyDomain::TREASURY,
    };

    std::set<std::string> tags;
    for (const auto domain : domains) tags.emplace(cybou::KeyDomainTag(domain));
    BOOST_CHECK_EQUAL(tags.size(), domains.size());
}

BOOST_AUTO_TEST_CASE(account_id_has_one_canonical_fixed_width_encoding)
{
    std::array<unsigned char, cybou::AccountId::SIZE> bytes{};
    bytes.front() = 0x2a;

    const auto id{cybou::AccountId::FromBytes(bytes)};
    BOOST_REQUIRE(id);
    BOOST_CHECK_EQUAL(*id->Value().begin(), 0x2a);

    BOOST_CHECK(!cybou::AccountId::FromBytes(std::span{bytes}.first(bytes.size() - 1)));
    bytes.fill(0);
    BOOST_CHECK(!cybou::AccountId::FromBytes(bytes));
    BOOST_CHECK(cybou::AccountId{}.IsNull());
}

BOOST_AUTO_TEST_CASE(identity_authorization_serialization_and_commitment)
{
    std::array<unsigned char, 32> root_seed{};
    root_seed.fill(1);
    std::array<unsigned char, 32> dev_seed{};
    dev_seed.fill(2);
    const auto root = cybou::DeriveIdentityPublicKey(root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto dev = cybou::DeriveIdentityPublicKey(dev_seed, cybou::IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(root && dev);
    const cybou::IdentityAuthorization auth{*root, *dev};
    const auto commitment = cybou::ComputeIdentityAuthorizationCommitment(auth);
    BOOST_REQUIRE(commitment.has_value());
    const auto bytes = cybou::SerializeIdentityAuthorization(auth);
    BOOST_REQUIRE(bytes.has_value());
    const auto decoded = cybou::DeserializeIdentityAuthorization(*bytes);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == auth);
}

BOOST_AUTO_TEST_CASE(keystore_crash_safe_persistence_and_legacy_migration)
{
    const auto test_dir = std::filesystem::temp_directory_path() / "cybou_keystore_test";
    std::filesystem::create_directories(test_dir);
    const auto key_path = test_dir / "test_identity.key";
    const auto bak_path = test_dir / "test_identity.key.bak";
    std::filesystem::remove(key_path);
    std::filesystem::remove(bak_path);

    // 1. Generate new and save
    cybou::CybouKeyStore ks1;
    BOOST_REQUIRE(ks1.GenerateNew());
    const auto acc_id1 = ks1.GetAccountId();
    BOOST_REQUIRE(acc_id1.has_value());
    BOOST_REQUIRE(ks1.SaveToFile(key_path));
    BOOST_CHECK(std::filesystem::exists(key_path));

    // 2. Load into new keystore
    cybou::CybouKeyStore ks2;
    BOOST_REQUIRE(ks2.LoadFromFile(key_path));
    BOOST_CHECK(ks2.GetAccountId() == acc_id1);

    // 3. Save again -> creates .bak
    BOOST_REQUIRE(ks2.SaveToFile(key_path));
    BOOST_CHECK(std::filesystem::exists(bak_path));

    // 4. Test legacy 32-byte raw migration
    const auto legacy_path = test_dir / "legacy_identity.key";
    std::filesystem::remove(legacy_path);
    std::array<unsigned char, 32> raw_seed{};
    raw_seed.fill(0x5a);
    {
        std::ofstream out(legacy_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(raw_seed.data()), raw_seed.size());
    }
    BOOST_CHECK_EQUAL(std::filesystem::file_size(legacy_path), 32);

    cybou::CybouKeyStore ks_legacy;
    BOOST_REQUIRE(ks_legacy.LoadFromFile(legacy_path));
    BOOST_CHECK(ks_legacy.HasKey());
    // Auto-migrated to wrapped format
    BOOST_CHECK(std::filesystem::file_size(legacy_path) > 32);

    std::filesystem::remove_all(test_dir);
}

BOOST_AUTO_TEST_SUITE_END()
