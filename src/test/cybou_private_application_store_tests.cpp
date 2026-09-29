// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/private_application_store.h>
#include <cybou/identity_material.h>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct TemporaryStore {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("cybou-private-application-store-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryStore() { std::filesystem::create_directories(path); }
    ~TemporaryStore() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};

std::vector<unsigned char> Bytes(const std::string_view text)
{
    return {text.begin(), text.end()};
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_private_application_store_tests)

BOOST_AUTO_TEST_CASE(encrypted_projection_survives_restart_and_does_not_contain_plaintext)
{
    TemporaryStore temporary;
    cybou::CybouKeyStore identity;
    BOOST_REQUIRE(identity.GenerateNew());
    const auto value = Bytes("secret-mail-subject-792640");
    std::filesystem::path database_path;
    {
        cybou::PrivateApplicationStore store{identity, temporary.path};
        database_path = store.Path();
        BOOST_CHECK(store.IsUnlocked());
        BOOST_CHECK(store.Put("mail/inbox/1", value));
        BOOST_CHECK(store.Get("mail/inbox/1") == value);
        BOOST_CHECK(!store.Get("mail/inbox/2"));
    }
    {
        cybou::PrivateApplicationStore store{identity, temporary.path};
        BOOST_CHECK(store.Get("mail/inbox/1") == value);
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(database_path)) {
        if (!entry.is_regular_file()) continue;
        std::ifstream input{entry.path(), std::ios::binary};
        const std::string raw{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        BOOST_CHECK(raw.find("secret-mail-subject-792640") == std::string::npos);
        BOOST_CHECK(raw.find("mail/inbox/1") == std::string::npos);
    }
    std::filesystem::remove_all(database_path);
    cybou::PrivateApplicationStore rebuilt{identity, temporary.path};
    BOOST_CHECK(!rebuilt.Get("mail/inbox/1"));
    BOOST_CHECK(rebuilt.Put("files/item/1", Bytes("file")));
}

BOOST_AUTO_TEST_CASE(lock_and_identity_switch_deny_access)
{
    TemporaryStore temporary;
    cybou::CybouKeyStore identity;
    BOOST_REQUIRE(identity.GenerateNew());
    const auto original_account = identity.GetAccountId();
    BOOST_REQUIRE(original_account);
    const auto vault = temporary.path / "identity.cybv2";
    BOOST_REQUIRE(identity.SaveToFile(vault, "test-password"));
    cybou::PrivateApplicationStore store{identity, temporary.path};
    BOOST_CHECK(store.Put("mail/sent/1", Bytes("private")));

    identity.Clear();
    BOOST_CHECK(!store.IsUnlocked());
    BOOST_CHECK(!store.Get("mail/sent/1"));
    BOOST_CHECK(!store.Put("mail/sent/2", Bytes("blocked")));
    BOOST_CHECK(!store.Erase("mail/sent/1"));

    BOOST_REQUIRE(identity.GenerateNew());
    BOOST_CHECK(identity.GetAccountId() != original_account);
    BOOST_CHECK(!store.IsUnlocked());
    BOOST_CHECK(!store.Get("mail/sent/1"));
    {
        cybou::PrivateApplicationStore other{identity, temporary.path};
        BOOST_CHECK(!other.Get("mail/sent/1"));
    }

    identity.Clear();
    BOOST_REQUIRE(identity.LoadFromFile(vault, "test-password"));
    BOOST_CHECK(store.IsUnlocked());
    BOOST_CHECK(store.Get("mail/sent/1") == Bytes("private"));
}

BOOST_AUTO_TEST_CASE(same_account_with_wrong_entropy_cannot_open_or_use_store)
{
    TemporaryStore temporary;
    cybou::CybouKeyStore identity;
    BOOST_REQUIRE(identity.GenerateNew());
    const auto account = identity.GetAccountId();
    BOOST_REQUIRE(account);
    cybou::PrivateApplicationStore store{identity, temporary.path};
    BOOST_CHECK(store.Put("scan/checkpoint", Bytes("42")));

    auto wrong = cybou::GenerateIdentityMaterial();
    BOOST_REQUIRE(wrong);
    std::copy(account->Value().begin(), account->Value().end(), wrong->account_id.begin());
    cybou::CybouKeyStore impostor;
    BOOST_REQUIRE(impostor.LoadMaterial(std::move(*wrong)));
    BOOST_CHECK_THROW((cybou::PrivateApplicationStore{impostor, temporary.path}), std::runtime_error);
    identity = std::move(impostor);
    BOOST_CHECK(!store.IsUnlocked());
    BOOST_CHECK(!store.Get("scan/checkpoint"));
}

BOOST_AUTO_TEST_SUITE_END()
