// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

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

BOOST_AUTO_TEST_CASE(identity_directory_layout_is_flat)
{
    TemporaryStore temporary;
    cybou::CybouKeyStore identity;
    BOOST_REQUIRE(identity.GenerateNew());
    const auto account = *identity.GetAccountId();
    const auto dir = cybou::IdentityDataDirectory(temporary.path, account);
    // <data>/identities/<AccountID hex, canonical byte order>/app.db
    BOOST_CHECK(dir.parent_path() == temporary.path / "identities");
    std::string hex;
    for (const auto byte : std::span{account.Value().begin(), cybou::AccountId::SIZE}) {
        static constexpr char DIGITS[]{"0123456789abcdef"};
        hex += DIGITS[byte >> 4];
        hex += DIGITS[byte & 0x0f];
    }
    BOOST_CHECK_EQUAL(dir.filename().string(), hex);
    cybou::PrivateApplicationStore store{identity, dir};
    BOOST_CHECK(store.Path() == dir / "app.db");
    BOOST_CHECK(std::filesystem::is_directory(dir / "app.db"));
}

BOOST_AUTO_TEST_CASE(lock_and_identity_switch_deny_access)
{
    TemporaryStore temporary;
    cybou::CybouKeyStore identity;
    BOOST_REQUIRE(identity.GenerateNew());
    const auto original_account = identity.GetAccountId();
    BOOST_REQUIRE(original_account);
    const auto vault = temporary.path / "identity.vault";
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
        cybou::PrivateApplicationStore other{identity, cybou::IdentityDataDirectory(temporary.path, *identity.GetAccountId())};
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

BOOST_AUTO_TEST_CASE(batches_persist_all_changes_or_none)
{
    TemporaryStore temporary;
    cybou::CybouKeyStore identity;
    BOOST_REQUIRE(identity.GenerateNew());
    {
        cybou::PrivateApplicationStore store{identity, temporary.path};
        BOOST_REQUIRE(store.Put("old", Bytes("remove me")));
        {
            // A "crash" before Commit: nothing staged may reach disk.
            cybou::PrivateApplicationStore::Batch batch{store};
            BOOST_REQUIRE(store.Put("record", Bytes("payload")));
            BOOST_REQUIRE(store.Put("index", Bytes("record")));
            BOOST_REQUIRE(store.Erase("old"));
            // Reads inside the batch see the staged state.
            BOOST_CHECK(store.Get("record") == Bytes("payload"));
            BOOST_CHECK(!store.Has("old"));
        }
        BOOST_CHECK(!store.Get("record"));
        BOOST_CHECK(!store.Get("index"));
        BOOST_CHECK(store.Get("old") == Bytes("remove me"));
        {
            cybou::PrivateApplicationStore::Batch batch{store};
            BOOST_REQUIRE(store.Put("record", Bytes("payload")));
            {
                cybou::PrivateApplicationStore::Batch nested{store}; // joins the outer batch
                BOOST_REQUIRE(store.Put("index", Bytes("record")));
                BOOST_REQUIRE(nested.Commit());
            }
            {
                // An abandoned savepoint rolls back only its own changes...
                cybou::PrivateApplicationStore::Batch abandoned{store};
                BOOST_REQUIRE(store.Put("record", Bytes("overwritten")));
                BOOST_REQUIRE(store.Put("partial", Bytes("half done")));
            }
            BOOST_CHECK(store.Get("record") == Bytes("payload"));
            BOOST_CHECK(!store.Has("partial"));
            BOOST_REQUIRE(store.Erase("old"));
            // ...and the enclosing batch still commits everything else.
            BOOST_REQUIRE(batch.Commit());
        }
    }
    // After reopening (a restart) both writes and the erase are present together.
    cybou::PrivateApplicationStore reopened{identity, temporary.path};
    BOOST_CHECK(reopened.Get("record") == Bytes("payload"));
    BOOST_CHECK(reopened.Get("index") == Bytes("record"));
    BOOST_CHECK(!reopened.Has("old"));
    BOOST_CHECK(!reopened.Has("partial"));
    const std::vector<cybou::PrivateApplicationStore::Change> changes{
        {"a", Bytes("1")}, {"b", Bytes("2")}, {"record", std::nullopt}};
    BOOST_REQUIRE(reopened.WriteBatch(changes));
    BOOST_CHECK(reopened.Get("a") == Bytes("1"));
    BOOST_CHECK(!reopened.Has("record"));
}

BOOST_AUTO_TEST_SUITE_END()
