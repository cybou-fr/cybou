// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_vault.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_identity_vault_tests)

BOOST_AUTO_TEST_CASE(portable_envelope_authenticates_header_and_payload)
{
    const std::vector<unsigned char> secret{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto sealed = cybou::SealIdentityVault("correct horse battery", secret);
    BOOST_REQUIRE(sealed);
    BOOST_CHECK(cybou::OpenIdentityVault("correct horse battery", *sealed) == secret);
    BOOST_CHECK(!cybou::OpenIdentityVault("incorrect password", *sealed));
    BOOST_CHECK(std::search(sealed->begin(), sealed->end(), secret.begin(), secret.end()) == sealed->end());

    for (const size_t offset : {size_t{0}, size_t{5}, size_t{20}, size_t{40}, size_t{60}, size_t{61}, sealed->size() - 1}) {
        auto tampered = *sealed;
        tampered[offset] ^= 1;
        BOOST_CHECK(!cybou::OpenIdentityVault("correct horse battery", tampered));
    }
    sealed->pop_back();
    BOOST_CHECK(!cybou::OpenIdentityVault("correct horse battery", *sealed));
}

BOOST_AUTO_TEST_CASE(password_and_payload_bounds)
{
    const std::array<unsigned char, 1> payload{42};
    BOOST_CHECK(!cybou::SealIdentityVault("short", payload));
    BOOST_CHECK(!cybou::SealIdentityVault("correct horse battery", std::span<const unsigned char>{}));
    const std::vector<unsigned char> oversized(65537, 0);
    BOOST_CHECK(!cybou::SealIdentityVault("correct horse battery", oversized));
}

BOOST_AUTO_TEST_CASE(new_file_is_durable_authenticated_and_never_overwritten)
{
    const auto path = std::filesystem::temp_directory_path() / "cybou_identity_vault_file_test.cybv2";
    std::filesystem::remove(path);
    const std::vector<unsigned char> secret{11, 22, 33, 44};
    BOOST_REQUIRE(cybou::SaveNewIdentityVault(path, "correct horse battery", secret));
    BOOST_CHECK(cybou::LoadIdentityVault(path, "correct horse battery") == secret);
    BOOST_CHECK(!cybou::LoadIdentityVault(path, "incorrect password"));
    BOOST_CHECK(!cybou::SaveNewIdentityVault(path, "correct horse battery", secret));
    BOOST_CHECK(cybou::LoadIdentityVault(path, "correct horse battery") == secret);

    std::vector<unsigned char> bytes;
    {
        std::ifstream in(path, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    BOOST_REQUIRE(bytes.size() > 100);
    bytes.back() ^= 1;
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    BOOST_CHECK(!cybou::LoadIdentityVault(path, "correct horse battery"));
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_SUITE_END()
